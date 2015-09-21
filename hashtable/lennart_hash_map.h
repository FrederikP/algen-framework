#pragma once

#include <cassert>

#include "../common/contenders.h"
#include "hashtable.h"
#include <primesieve.hpp>

using namespace common::monad;

namespace hashtable {

class bucket_hash_function {
private:
	size_t random;
	size_t prime;
	size_t size;
	
public:
	void initialize(size_t capacity, size_t bucketAmount) {
		std::vector<size_t> primes;
		// Choose first prime >= capacity
		primesieve::generate_n_primes<size_t>(1, capacity, &primes);
		prime = primes[0];
		
		size = bucketAmount;
		
		// Seed with a real random value, if available
		std::random_device device;
		// Choose a random factor between 1 and prime - 1
		std::default_random_engine engine(device());
		std::uniform_int_distribution<size_t> uniform_dist(1, prime - 1);
		random = uniform_dist(engine);
	}
	size_t operator()(size_t& x) const {
		return (random * x % prime) % size;
	}
};

class element_hash_function {
private:
	size_t random;
	size_t prime;
public:
	void initialize(size_t size) {
		std::vector<size_t> primes;
		// Choose first prime >= M
		primesieve::generate_n_primes<size_t>(1, size, &primes);
		prime = primes[0];
		
		// Seed with a real random value, if available
		std::random_device device;
		// Choose a random factor between 1 and prime - 1
		std::default_random_engine engine(device());
		std::uniform_int_distribution<size_t> uniform_dist(1, prime - 1);
		random = uniform_dist(engine);
	}
	size_t getPrime() {
		return prime;
	}
	size_t operator()(size_t& x) const {
		return random * x % prime;
	}
};

template <typename Key, typename T>
class element {
private:
	Key _key;
	T _value;
	bool initialized;
	bool deleteFlag;
public:
	element() {
		_key = 0;
		_value = 0;
		initialized = false;
		deleteFlag = false;
	}
	element(Key& key, T& value) {
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

	void initialize(Key& key) {
		_key = key;
		initialized = true;
	}

	bool isDeleted() {
		return deleteFlag;
	}

	void markDeleted() {
		deleteFlag = true;
	}
};

#define _unused(x) ((void)x) // Makro to prevent unused parameter errors
template <typename Key, typename T>
class bucket {
private:
	element_hash_function elementHashFunction;
	size_t size;
	size_t _count;
	element<Key, T>* elements;
public:
	bucket() : bucket(0) { }
	bucket(size_t initialElementAmount) {
		_count = 0;
		size_t min = 10;
		size_t max = std::max(min, initialElementAmount);
		size_t intendedSize = max * (max - 1) + 1;
		elementHashFunction.initialize(intendedSize);
		size = elementHashFunction.getPrime();

		elements = new element<Key, T>[size];
		for(size_t i = 0; i < size; i++) {
			elements[i] = element<Key, T>();
		}
	}
	
	T& getValue(size_t preHash, Key key) {
		size_t elementIndex = elementHashFunction(preHash);
		element<Key, T> element = elements[elementIndex];
		if (!element.isInitialized()) {
			element.initialize(key);
			_count++;
		}
		// If this is not the case something with the dynamic rehashing didn't work out
		assert(element.getKey() == key);
		_unused(key);
		return element.getValue();
	}

	maybe<T> find(size_t preHash, Key key) const {
		size_t elementIndex = elementHashFunction(preHash);
		element<Key, T> element = elements[elementIndex];
		
		if (element.isInitialized() and !element.isDeleted()) {
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(element.getKey() == key);
			_unused(key);
			return just<T>(element.getValue());
		}
		return nothing<T>();
	}
	
	size_t erase(size_t preHash, Key key) {
		size_t elementIndex = elementHashFunction(preHash);
		element<Key, T> element = elements[elementIndex];

		if (element.isInitialized() and !element.isDeleted()) {
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(element.getKey() == key);
			_unused(key);
			element.markDeleted();
			_count--;
			return 1;
		}
		return 0;
	}

	size_t count() {
		return _count;
	}
};

template <typename Key, typename T, typename PreHashFcn = std::hash<Key>>
class DPH_with_array_buckets : public hashtable<Key, T> { // DPH = Dynamic Perfect Hashing
private:
	PreHashFcn preHashFcn;
	bucket_hash_function bucketHashFunction;
	size_t capacity;
	size_t bucketAmount;
	bucket<Key, T>* buckets;
	
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

		bucketHashFunction.initialize(capacity, bucketAmount);

		size_t initialElementPerBucketAmount = capacity / bucketAmount;
		buckets = new bucket<Key, T>[bucketAmount];
		for (size_t i = 0; i < bucketAmount; i++) {
			buckets[i] = bucket<Key, T>(initialElementPerBucketAmount);
		}
    }
		
    T& operator[](const Key &key) override {
		size_t preHash = preHashFcn(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket<Key, T> bucket = buckets[bucketIndex];
		return bucket.getValue(preHash, key);
	}

    T& operator[](Key&& key) override {
		Key movedKey = std::move(key);
		return (*this)[movedKey];
    }

    maybe<T> find(const Key &key) const override {
		size_t preHash = preHashFcn(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket<Key, T> bucket = buckets[bucketIndex];
		return bucket.find(preHash, key);
    }

    size_t erase(const Key &key) override {
		size_t preHash = preHashFcn(std::move(key));
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket<Key, T> bucket = buckets[bucketIndex];
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

	}
};

}
