#pragma once

#include <cassert>

#include "../common/contenders.h"
#include "hashtable.h"
#include "DPH_Common.h"

using namespace common::monad;

namespace hashtable {

class bucket_info {
public:
	size_t M;
	size_t b;
	size_t start;
	size_t length;
	size_t count;
	hash_function hashFunction;

	bucket_info() { }

	bucket_info(size_t bucketM, size_t bucketStart, size_t bucketLength, size_t random, size_t prime) {
		M = bucketM;
		b = 0;
		start = bucketStart;
		length = bucketLength;
		count = 0;
		hashFunction.setParameters(random, prime, length);
	}

	size_t index(size_t preHash) const {
		size_t index = start + hashFunction(preHash);
		assert(index < start + length);
		return index;
	}
};

template <typename Key, typename T,
		  typename PreHashFcn = std::hash<Key>>
class DPH_with_single_vector : public hashtable<Key, T> { // DPH = Dynamic Perfect Hashing
private:
	size_t c;
	size_t M;
	size_t bucketAmount;
	size_t _elementAmount;

	prime_generator primes;
	random_generator randoms;
	
	PreHashFcn preHashFunction;
	hash_function bucketHashFunction;
	std::vector<bucket_info> bucketInfos;
	std::vector< bucket_entry< Key, T > > entries;
	
public:
    virtual ~DPH_with_single_vector() = default;

    // Register all contenders in the list
    static void register_contenders(common::contender_list<hashtable<Key, T>> &list) {
        using Factory = common::contender_factory<hashtable<Key, T>>;
        list.register_contender(Factory("DPH_with_single_vector", "DPH_with_single_vector",
            [](){ 
				size_t initialElementAmount = 1000;
				size_t initialBucketAmount = 100;
				return new DPH_with_single_vector(initialElementAmount, initialBucketAmount); 
			}
        ));
    }
	
    DPH_with_single_vector(size_t initialElementAmount, size_t initialBucketAmount) : hashtable<Key, T>(), bucketInfos(initialBucketAmount) {
		c = 1;
		M = calculateM(initialElementAmount);
		_elementAmount = 0;
		bucketAmount = initialBucketAmount;

		size_t prime = primes(initialElementAmount);
		size_t random = randoms(1, prime - 1);
		bucketHashFunction.setParameters(random, prime, bucketAmount);

		size_t initialElementPerBucketAmount = initialElementAmount / initialBucketAmount;
		size_t bucketM = std::max(size_t(10), initialElementPerBucketAmount);
		size_t bucketLength = 2 * bucketM * (bucketM - 1);
		size_t bucketPrime = primes(bucketLength);
		for(size_t i = 0; i < bucketAmount; i++) {
			size_t bucketStart = i == 0 ? 0 : i * bucketLength;
			size_t random = randoms(1, bucketPrime - 1);

			bucketInfos[i] = bucket_info(bucketM, bucketStart, bucketLength, random, bucketPrime);
		}

		entries = std::vector< bucket_entry< Key, T > >(bucketAmount * bucketLength);
    }
		
    T& operator[](const Key &key) override {
		size_t preHash = preHashFunction(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		size_t elementIndex = bucketInfos[bucketIndex].index(preHash);
		bucket_entry<Key, T>& entry = entries[elementIndex];
		if (!entry.isInitialized()) {
			entry.initialize(key);
			_elementAmount++;
			M++;
		} else if (entry.isDeleted()) {
			entry = bucket_entry<Key, T>();
			entry.initialize(key);
			_elementAmount++;
		}
		// If this is not the case something with the dynamic rehashing didn't work out
		assert(entry.getKey() == key);
		return entry.getValue();
	}

    T& operator[](Key&& key) override {
		Key movedKey = std::move(key);
		return (*this)[movedKey];
    }

    maybe<T> find(const Key &key) const override {
		size_t preHash = preHashFunction(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		size_t elementIndex = bucketInfos[bucketIndex].index(preHash);
		bucket_entry<Key, T> entry = entries[elementIndex];
		if (entry.isInitialized() and !entry.isDeleted()) {
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(entry.getKey() == key);
			return just<T>(entry.getValue());
		}
		return nothing<T>();
    }

    size_t erase(const Key &key) override {
		size_t preHash = preHashFunction(std::move(key));
		size_t bucketIndex = bucketHashFunction(preHash);
		size_t elementIndex = bucketInfos[bucketIndex].index(preHash);
		bucket_entry<Key, T>& entry = entries[elementIndex];

		if (entry.isInitialized() and !entry.isDeleted()) {
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(entry.getKey() == key);
			entry.markDeleted();
			_elementAmount--;
			return 1;
		}
		return 0;
    }

    size_t size() const override {
		return _elementAmount;
	}

    void clear() override { 
		size_t entriesCapacity = entries.capacity();
		entries = std::vector< bucket_entry< Key, T > >(entriesCapacity);
		_elementAmount = 0;
	}

private:
	size_t calculateM(size_t elementAmount) {
		return (1 + c) + elementAmount;
	}
};

}