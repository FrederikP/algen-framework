#pragma once

#include <cassert>
#include <iostream>

#include <primesieve.hpp>

using namespace common::monad;

namespace hashtable {

class hash_function {
private:
	size_t _random;
	size_t _random2;
	size_t _prime;
	size_t _size;
	
public:
	hash_function() : hash_function(0, 0, 0, 0) { }
	hash_function(size_t random, size_t random2, size_t prime, size_t size) {
		setParameters(random, random2, prime, size);
	}
	void setParameters(size_t random, size_t random2, size_t prime, size_t size) {
		_random = random;
		_random2 = random2;
		_prime = prime;
		_size = size;
	}
	size_t operator()(size_t& x) const {
		assert(_random >= size_t(1) and _random <= (_prime - 1));
		assert(_prime >= _size);
		size_t product = _random * x;
		size_t sum = product + _random2;
		size_t primed = sum % _prime;
		size_t sized = primed % _size;
		return sized;
	}
};

class prime_generator {
public:
	size_t operator()(size_t greaterEqualsThan) {
		std::vector<size_t> primes;
		primesieve::generate_n_primes<size_t>(1, greaterEqualsThan, &primes);
		return primes[0];
	}
};

class random_generator {
public:
	size_t operator()(size_t from, size_t to) {
		// Seed with a real random value, if available
		std::random_device device;
		std::default_random_engine engine(device());
		std::uniform_int_distribution<size_t> uniform_dist(from, to);
		return uniform_dist(engine);
	}
};

template <typename Key, typename T>
class bucket_entry {
private:
	Key _key;
	T _value;
	bool initialized;
	bool deleteFlag;
public:
	bucket_entry() {
		_key = Key();
		_value = T();
		initialized = false;
		deleteFlag = false;
	}
	bucket_entry(Key& key, T& value) {
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

	void markDeleted() {
		deleteFlag = true;
	}
};

class rehash_counters {
public:
	int resizeAndRehashBucketCounter;
	int rehashBucketCounter;
	int rehashBucketNewFunctionCounter;
	int rehashAllCounter;
	int rehashAllNewFunctionCounter;
	int rehashAllNewBucketFunctionCounter;

	rehash_counters() {
		resizeAndRehashBucketCounter = 0;
		rehashBucketCounter = 0;
		rehashBucketNewFunctionCounter = 0;
		rehashAllCounter = 0;
		rehashAllNewFunctionCounter = 0;
		rehashAllNewBucketFunctionCounter = 0;
	}

	double rehashBucketNewFunctionRatio() {
		return rehashBucketCounter / (double) rehashBucketNewFunctionCounter;
	}

	double rehashAllNewFunctionRatio() {
		return rehashAllCounter / (double) rehashAllNewFunctionCounter;
	}

	double rehashAllNewBucketFunctionRatio() {
		return rehashAllCounter / (double) rehashAllNewBucketFunctionCounter;
	}

	void print() {
		std::cout << "Resize and Rehash Bucket: " << resizeAndRehashBucketCounter << std::endl;
		std::cout << "Rehash Bucket: " << rehashBucketCounter << std::endl;
		std::cout << "Rehash Bucket New Function Ratio: " << rehashBucketNewFunctionRatio() << std::endl;
		std::cout << "Rehash All: " << rehashAllCounter << std::endl;
		std::cout << "Rehash All New Function Ratio: " << rehashAllNewFunctionRatio() << std::endl;
		std::cout << "Rehash All New Bucket Function Ratio: " << rehashAllNewBucketFunctionRatio() << std::endl;
		std::cout << std::endl;
	}
};

}
