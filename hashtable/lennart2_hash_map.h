#pragma once

#include <cassert>

#include "../common/contenders.h"
#include "hashtable.h"
#include <primesieve.hpp>

using namespace common::monad;

namespace hashtable {

class sized_hash_function {
private:
	size_t _random;
	size_t _prime;
	size_t _size;
	
public:
	void initialize(size_t random, size_t prime, size_t size) {
		_random = random;
		_prime = prime;
		_size = size;
	}
	size_t operator()(size_t& x) const {
		return (_random * x % _prime) % _size;
	}
};

class simple_hash_function {
private:
	size_t _random;
	size_t _prime;
public:
	simple_hash_function(size_t random, size_t prime) {
		_random = random;
		_prime = prime;
	}
	size_t operator()(size_t& x) const {
		return _random * x % _prime;
	}
};

template <typename Key, typename T>
class value_entry {
private:
	Key _key;
	T _value;
	bool initialized;
	bool deleteFlag;
public:
	value_entry() {
		_key = Key();
		_value = T();
		initialized = false;
		deleteFlag = false;
	}
	value_entry(Key& key, T& value) {
		_key = key;
		_value = value;
		initialized = false;
		deleteFlag = false;
	}
	
	Key& getKey() {
		return _key;
	}
	
	T& getValue() {
		return _value;
	}

	bool isInitialized() {
		return initialized;
	}

	void initialize(Key key) {
		_key = key;
		initialized = true;
	}

	bool isDeleted() {
		return deleteFlag;
	}

	void setDeleted(bool deleted) {
		deleteFlag = deleted;
	}
};

class bucket_info {
public:
	size_t start;
	size_t length;
	simple_hash_function hashFunction;

	bucket_info() : hashFunction(0, 0) { }

	bucket_info(size_t bucketStart, size_t primeLength, size_t random) : hashFunction(random, primeLength) {
		start = bucketStart;
		length = primeLength;
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
	size_t capacity;
	size_t count;
	size_t bucketAmount;

	PreHashFcn preHashFunction;
	sized_hash_function bucketHashFunction;
	std::vector<bucket_info> bucketInfos;
	std::vector< value_entry< Key, T > > entries;
	
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
		capacity = initialElementAmount;
		count = 0;
		bucketAmount = initialBucketAmount;

		size_t prime = getPrime(capacity);
		size_t random = getRandom(1, prime - 1);
		bucketHashFunction.initialize(random, prime, bucketAmount);

		size_t initialElementPerBucketAmount = initialElementAmount / initialBucketAmount;
		size_t intendedBucketLength = std::max(size_t(10), initialElementPerBucketAmount);
		intendedBucketLength = intendedBucketLength * (intendedBucketLength - 1) + 1;
		size_t initialBucketLength = getPrime(intendedBucketLength);
		for(size_t i = 0; i < bucketAmount; i++) {
			size_t bucketStart = i * initialBucketLength;
			size_t bucketLength = initialBucketLength;
			size_t random = getRandom(1, initialBucketLength - 1);

			bucketInfos[i] = bucket_info(bucketStart, bucketLength, random);
		}

		entries = std::vector< value_entry< Key, T > >(bucketAmount * initialBucketLength);
    }
		
    T& operator[](const Key &key) override {
		size_t preHash = preHashFunction(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		size_t elementIndex = bucketInfos[bucketIndex].index(preHash);
		value_entry<Key, T>& entry = entries[elementIndex];
		if (!entry.isInitialized()) {
			entry.initialize(key);
			count++;
		} else if (entry.isDeleted()) {
			entry = value_entry<Key, T>();
			entry.initialize(key);
			count++;
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
		value_entry<Key, T> entry = entries[elementIndex];
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
		value_entry<Key, T>& entry = entries[elementIndex];

		if (entry.isInitialized() and !entry.isDeleted()) {
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(entry.getKey() == key);
			entry.setDeleted(true);
			count--;
			return 1;
		}
		return 0;
    }

    size_t size() const override {
		return count;
	}

    void clear() override { 
		size_t entriesCapacity = entries.capacity();
		entries = std::vector< value_entry< Key, T > >(entriesCapacity);
		count = 0;
	}

private:
	size_t getPrime(size_t greaterEqualsThan) {
		std::vector<size_t> primes;
		primesieve::generate_n_primes<size_t>(1, greaterEqualsThan, &primes);
		return primes[0];
	}

	size_t getRandom(size_t from, size_t to) {
		// Seed with a real random value, if available
		std::random_device device;
		// Choose a random factor between 1 and prime - 1
		std::default_random_engine engine(device());
		std::uniform_int_distribution<size_t> uniform_dist(from, to);
		return uniform_dist(engine);
	}
};

}
