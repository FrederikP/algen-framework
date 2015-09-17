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
	size_t bucketAmount;
	
public:
	bucket_hash_function(size_t M, size_t bucketAmount) {
		std::vector<size_t> primes;
		// Choose first prime >= M
		primesieve::generate_n_primes<size_t>(1, M, &primes);
		prime = primes[0];
		bucketAmount = bucketAmount;
		// Seed with a real random value, if available
		std::random_device device;
 
		// Choose a random factor between 1 and prime - 1
		std::default_random_engine engine(device());
		std::uniform_int_distribution<size_t> uniform_dist(1, prime - 1);
		random = uniform_dist(engine);
	}
	size_t operator()(size_t& x) const {
		return (random * x % prime) % bucketAmount;
	}
};

class element_hash_function {
public:
	size_t operator()(size_t& x) const {
		return x * 0;
	}
};

template <typename Key, typename T>
class element {
private:
	Key key;
	T value;
	bool deleteFlag;
public:
	element(Key& key, T& value) : key(key), value(value), deleteFlag(false) { }
	element() : key(0), value(0), deleteFlag(false) {}
	
	Key& getKey() {
		return key;
	}
	
	T& getValue() {
		return value;
	}

	bool isDeleted() {
		return deleteFlag;
	}

	void markDeleted() {
		deleteFlag = true;
	}
};

template <typename Key, typename T>
class bucket {
private:
	element_hash_function elementHashFunction;
	size_t size;
	element<Key, T> elements[];
public:
	bucket() : bucket(0) { }
	bucket(size_t intendedSize) {
		size_t minimumSize = 10;
		size_t max = std::max(minimumSize, intendedSize);
		size = max * (max - 1) + 1;
		elements[size] = { };
	}
	
	T& getValue(size_t key) {
		size_t elementIndex = elementHashFunction(key);
		element<Key, T> element = elements[elementIndex];
		return element.getValue();
	}
};

template <typename Key, typename T, typename PreHashFcn = std::hash<Key>>
class DPH_with_array_buckets : public hashtable<Key, T> { // DPH = Dynamic Perfect Hashing
private:
	PreHashFcn preHashFcn;
	bucket_hash_function bucketHashFunction;
	size_t M;
	size_t count;
	size_t bucketAmount;
	bucket<Key, T> buckets[];
	
public:
    virtual ~DPH_with_array_buckets() = default;

    // Register all contenders in the list
    static void register_contenders(common::contender_list<hashtable<Key, T>> &list) {
        using Factory = common::contender_factory<hashtable<Key, T>>;
		size_t initialM = 10;
		size_t s = 100;
        list.register_contender(Factory("DPH_with_array_buckets", "DPH_with_array_buckets",
            [initialM, s](){ return new DPH_with_array_buckets<Key, T>(initialM, s); }
        ));
    }
	
    DPH_with_array_buckets(size_t initialM, size_t initialBucketAmount)
			: hashtable<Key, T>(), bucketHashFunction(initialM, initialBucketAmount) {
		M = initialM;
		count = 0;
		bucketAmount = initialBucketAmount;
		
		buckets[bucketAmount] = { };
		for (size_t i = 0; i < bucketAmount; i++) {
			buckets[i] = bucket<Key, T>();
		}
    }
		
    T& operator[](const Key &key) override {
		size_t preHash = preHashFcn(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket<Key, T> bucket = buckets[bucketIndex];
		return bucket.getValue(preHash);
	}

    T& operator[](Key&& key) override {
		size_t preHash = preHashFcn(std::move(key));
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket<Key, T> bucket = buckets[bucketIndex];
		return bucket.getValue(preHash);
    }

    maybe<T> find(const Key &key) const override {
		/*size_t preHash =*/ preHashFcn(key);
		return nothing<T>();
    }

    size_t erase(const Key &key) override {
		/*size_t preHash = */preHashFcn(key);
		return 0;
    }

    size_t size() const override { 
		return count;
	}

    void clear() override { 

	}
};

}
