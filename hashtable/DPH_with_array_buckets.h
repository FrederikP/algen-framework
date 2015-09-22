#pragma once

#include <cassert>

#include "../common/contenders.h"
#include "hashtable.h"
#include "DPH_Common.h"

using namespace common::monad;

namespace hashtable {

#define _unused(x) ((void)x) // Makro to prevent unused parameter errors
template <typename Key, typename T>
class array_bucket {
private:
	prime_generator primes;
	random_generator randoms;
	entry_hash_function entryHashFunction;
	size_t size;
	size_t _count;
	bucket_entry<Key, T>* entries;
public:
	array_bucket() : array_bucket(0) { }
	array_bucket(size_t initialElementAmount) {
		size_t intendedBucketLength = std::max(size_t(10), initialElementAmount);
		intendedBucketLength = intendedBucketLength * (intendedBucketLength - 1) + 1;
		size = primes(intendedBucketLength);

		size_t random = randoms(1, size - 1);
		entryHashFunction.setParameters(random, size);

		_count = 0;
		entries = new bucket_entry<Key, T>[size];
	}
	
	T& getValue(size_t preHash, Key key) {
		size_t elementIndex = entryHashFunction(preHash);
		bucket_entry<Key, T>& entry = entries[elementIndex];
		if (!entry.isInitialized()) {
			entry.initialize(key);
			_count++;
		} else if (entry.isDeleted()) {
			entry = bucket_entry<Key, T>();
			entry.initialize(key);
			_count++;
		}
		// If this is not the case something with the dynamic rehashing didn't work out
		assert(entry.getKey() == key);
		_unused(key);
		return entry.getValue();
	}

	maybe<T> find(size_t preHash, Key key) const {
		size_t elementIndex = entryHashFunction(preHash);
		bucket_entry<Key, T> entry = entries[elementIndex];
		
		if (entry.isInitialized() and !entry.isDeleted()) {
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(entry.getKey() == key);
			_unused(key);
			return just<T>(entry.getValue());
		}
		return nothing<T>();
	}
	
	size_t erase(size_t preHash, Key key) {
		size_t elementIndex = entryHashFunction(preHash);
		bucket_entry<Key, T>& entry = entries[elementIndex];

		if (entry.isInitialized() and !entry.isDeleted()) {
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(entry.getKey() == key);
			_unused(key);
			entry.markDeleted();
			_count--;
			return 1;
		}
		return 0;
	}

	size_t count() {
		return _count;
	}

	void clear() {
		entries = new bucket_entry<Key, T>[size];
		_count = 0;
	}
};

template <typename Key, typename T, typename PreHashFcn = std::hash<Key>>
class DPH_with_array_buckets : public hashtable<Key, T> { // DPH = Dynamic Perfect Hashing
private:
	prime_generator primes;
	random_generator randoms;
	PreHashFcn preHashFcn;
	bucket_hash_function bucketHashFunction;
	size_t capacity;
	size_t bucketAmount;
	array_bucket<Key, T>* buckets;
	
public:
    virtual ~DPH_with_array_buckets() = default;

    // Register all contenders in the list
    static void register_contenders(common::contender_list<hashtable<Key, T>> &list) {
        using Factory = common::contender_factory<hashtable<Key, T>>;
        list.register_contender(Factory("DPH_with_array_buckets", "DPH_with_array_buckets",
            [](){ 
				size_t initialElementAmount = 1000;
				size_t initialBucketAmount = 100;
				return new DPH_with_array_buckets(initialElementAmount, initialBucketAmount); 
			}
        ));
    }
	
    DPH_with_array_buckets(size_t initialElementAmount, size_t initialBucketAmount) : hashtable<Key, T>() {
		capacity = initialElementAmount;
		bucketAmount = initialBucketAmount;

		size_t prime = primes(capacity);
		size_t random = randoms(1, prime - 1);
		bucketHashFunction.setParameters(random, prime, bucketAmount);

		size_t initialElementPerBucketAmount = capacity / bucketAmount;
		buckets = new array_bucket<Key, T>[bucketAmount];
		for (size_t i = 0; i < bucketAmount; i++) {
			buckets[i] = array_bucket<Key, T>(initialElementPerBucketAmount);
		}
    }
		
    T& operator[](const Key &key) override {
		size_t preHash = preHashFcn(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		array_bucket<Key, T>& bucket = buckets[bucketIndex];
		return bucket.getValue(preHash, key);
	}

    T& operator[](Key&& key) override {
		Key movedKey = std::move(key);
		return (*this)[movedKey];
    }

    maybe<T> find(const Key &key) const override {
		size_t preHash = preHashFcn(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		array_bucket<Key, T>& bucket = buckets[bucketIndex];
		return bucket.find(preHash, key);
    }

    size_t erase(const Key &key) override {
		size_t preHash = preHashFcn(std::move(key));
		size_t bucketIndex = bucketHashFunction(preHash);
		array_bucket<Key, T>& bucket = buckets[bucketIndex];
		return bucket.erase(preHash, key);
    }

    size_t size() const override {
		size_t overallCount = 0;
		for (size_t i = 0; i < bucketAmount; i++) {
			overallCount += buckets[i].count();
		}
		return overallCount;
	}

    void clear() override {
		for (size_t i = 0; i < bucketAmount; i++) {
			buckets[i].clear();
		}
	}
};

}
