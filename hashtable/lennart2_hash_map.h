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
	void initialize(size_t random, size_t prime) {
		_random = random;
		_prime = prime;
	}
	size_t operator()(size_t& x) const {
		return _random * x % _prime;
	}
};

template <typename Key, typename T>
class entry {
private:
	Key _key;
	T _value;
	bool initialized;
	bool deleteFlag;
public:
	entry() {
		_key = 0;
		_value = 0;
		initialized = false;
		deleteFlag = false;
	}
	entry(Key& key, T& value) {
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

template <typename Key, typename T,
		  typename PreHashFcn = std::hash<Key>>
class DPH_with_single_vector : public hashtable<Key, T> { // DPH = Dynamic Perfect Hashing
private:
	size_t capacity;
	size_t count;
	size_t bucketAmount;

	PreHashFcn preHashFcn;
	sized_hash_function bucketHashFunction;
	
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
	
    DPH_with_single_vector(size_t initialElementAmount, size_t initialBucketAmount) : hashtable<Key, T>() {
		capacity = initialElementAmount;
		count = 0;
		bucketAmount = initialBucketAmount;

		size_t prime = getPrime(capacity);
		size_t random = getRandom(1, prime - 1);
		bucketHashFunction.initialize(random, prime, bucketAmount);
    }
		
    T& operator[](const Key &key) override {
		/*size_t preHash =*/ preHashFcn(key);
		return 0;
	}

    T& operator[](Key&& key) override {
		Key movedKey = std::move(key);
		return (*this)[movedKey];
    }

    maybe<T> find(const Key &key) const override {
		/*size_t preHash =*/ preHashFcn(key);
		return nothing<T>();
    }

    size_t erase(const Key &key) override {
		/*size_t preHash =*/ preHashFcn(std::move(key));
		return 0;
    }

    size_t size() const override {
		return count;
	}

    void clear() override { 

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
