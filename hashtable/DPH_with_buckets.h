#pragma once

#include <cassert>
#include <iostream>

#include "../common/contenders.h"
#include "hashtable.h"
#include "DPH_Common.h"

using namespace common::monad;

namespace hashtable {

template <typename Key, typename T,
		  typename PreHashFcn = std::hash<Key>>
class bucket {
public:
	size_t M;
	size_t b;
	size_t length;
	size_t elementAmount;

private:
	prime_generator primes;
	random_generator randoms;

	PreHashFcn preHashFunction;
	hash_function hashFunction;
	std::vector<bucket_entry<Key, T>> entries;

public:
	bucket() : bucket(0) { }

	bucket(std::vector<bucket_entry<Key, T>> initialEntries) : bucket(initialEntries.size()) {
		insertAll(initialEntries);
	}

	bucket(size_t initialSize) :
		M(std::max(size_t(10), initialSize)),
		b(0),
		length(calculateBucketLength(M)),
		elementAmount(0),
		entries(length)
	{
		size_t prime = primes(length);
		size_t random = randoms(1, prime - 1);
		size_t random2 = randoms(1, prime - 1);
		hashFunction.setParameters(random, random2, prime, length);
	}

	bucket_entry<Key, T>& operator[](size_t preHash) {
		size_t index = hashFunction(preHash);
		return entries[index];
	}

    virtual ~bucket() = default;

    std::vector<bucket_entry<Key, T>>& getEntries() {
    	return entries;
    }

    maybe<T> find(size_t preHash, const Key &key) const {
    	size_t index = hashFunction(preHash);
		return entries[index].find(key);
    }

	void resizeAndRehash(const Key& key) {
		M *= 2;
		length = calculateBucketLength(M);
		rehash(key);
	}

	void rehash(const Key& key) {
		// TODO Use copy_if with move_iterator?
		// Collecting entries of the bucket
		std::vector<bucket_entry<Key, T>> bucketEntries(elementAmount + 1);
		size_t j = 0;
		bool includesNewKey = false;
		for (size_t i = 0; i < entries.size(); ++i) {
			bucket_entry<Key, T>& entry = entries[i];
			if (entry.isInitialized() && !entry.isDeleted()) {
				if (entry.getKey() == key) {
					includesNewKey = true;
				}
				bucketEntries[j] = entry;
				++j;
			}
			entries[i] = bucket_entry<Key, T>();
		}
		if (!includesNewKey) {
			bucket_entry<Key, T> entry = bucket_entry<Key, T>();
			entry.initialize(key);
			bucketEntries[j] = entry;
		} else {
			bucketEntries.pop_back();
		}

		entries.clear();
		entries.resize(length);
		insertAll(bucketEntries);
	}

	// TODO should be private
	static size_t calculateBucketLength(size_t bucketM) {
		return 2 * bucketM * (bucketM - 1);
	}

private:
	void insertAll(std::vector<bucket_entry<Key, T>> bucketEntries) {
		// Choose a new injective hash function randomly
		bool isInjective;
		do {
			std::cout << "rehashBucket: Creating new entry hash function" << "\n";

			isInjective = true;
			size_t prime = primes(length);
			size_t random = randoms(1, prime - 1);
			size_t random2 = randoms(1, prime - 1);
			hashFunction.setParameters(random, random2, prime, length);

			std::vector<size_t> indices = std::vector<size_t>(length);
			for(size_t i = 0; i < bucketEntries.size(); ++i) {
				bucket_entry<Key, T>& entry = bucketEntries[i];
				if (entry.isInitialized()) {
					size_t preHash = preHashFunction(entry.getKey());
					size_t index = hashFunction(preHash);
					if (indices[index] != 0) {
						isInjective = false;
						break;
					} else {
						indices[index] = 1;
					}
				}
			}
		} while (!isInjective);

		// TODO Use move semantic instead of copying?
		// Inserting the entries in the table
		for(size_t i = 0; i < bucketEntries.size(); ++i) {
			bucket_entry<Key, T> entry = bucketEntries[i];
			size_t preHash = preHashFunction(entry.getKey());
			size_t index = hashFunction(preHash);
			entries[index] = entry;
		}

	}
};

template <typename Key, typename T,
		  typename PreHashFcn = std::hash<Key>>
class DPH_with_buckets : public hashtable<Key, T> { // DPH = Dynamic Perfect Hashing
private:
	static const size_t c = 5;

	size_t M;
	size_t count;
	
	size_t bucketAmount;
	size_t _elementAmount;

	prime_generator primes;
	random_generator randoms;
	
	PreHashFcn preHashFunction;
	hash_function bucketHashFunction;
	std::vector<bucket<Key, T>> buckets;
	
public:
    virtual ~DPH_with_buckets() = default;

    // Register all contenders in the list
    static void register_contenders(common::contender_list<hashtable<Key, T>> &list) {
        using Factory = common::contender_factory<hashtable<Key, T>>;
        list.register_contender(Factory("DPH_with_buckets", "DPH_with_buckets",
            [](){ 
				return new DPH_with_buckets(1000);
			}
        ));
    }
	
    DPH_with_buckets(size_t initialElementAmount) :
    	hashtable<Key, T>(),
		M(calculateM(initialElementAmount)),
		count(0),
		bucketAmount(calculateBucketAmount(initialElementAmount)),
		_elementAmount(0),
		primes(),
		randoms(),
    	buckets(bucketAmount, bucket<Key, T>(initialElementAmount / bucketAmount))
	{
		size_t prime = primes(initialElementAmount);
		size_t random = randoms(1, prime - 1);
		size_t random2 = randoms(1, prime - 1);
		bucketHashFunction.setParameters(random, random2, prime, bucketAmount);
    }
		
    T& operator[](const Key &key) override {
		size_t preHash = preHashFunction(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket<Key, T>& _bucket = buckets[bucketIndex];
		bucket_entry<Key, T>& entry = _bucket[preHash];
		if (!entry.isInitialized()) {
			entry.initialize(key);
			++_elementAmount;
			++count;
			++_bucket.b;
			++_bucket.elementAmount;
		} else if (entry.isDeleted()) {
			entry = bucket_entry<Key, T>();
			entry.initialize(key);
			++_elementAmount;
			++count;
			++_bucket.b;
			++_bucket.elementAmount;
		}
		bool wasRehashed = false;
		if (count >= M) {
			std::cout << "Rehash all count>=M: " << count << ">="<< M << "\n";
			rehashAll(key);
			wasRehashed = true;
		} else if (_bucket.b <= _bucket.M and entry.getKey() != key) {
			std::cout << "Rehashing bucket bucket.b <= bucket.M: " << _bucket.b << "<="<< _bucket.M << "\n";
			_bucket.rehash(key);
			wasRehashed= true;
		} else if (_bucket.b > _bucket.M) {
			//TODO Code duplication
			size_t newBucketM = _bucket.M * 2;
			size_t newBucketLength = _bucket.calculateBucketLength(newBucketM);
			if (globalConditionIsSatisfied(newBucketLength, bucketIndex)) {
				std::cout << "Resizing bucket bucket.b > bucket.M: " << _bucket.b << ">"<< _bucket.M << "\n";
				_bucket.resizeAndRehash(key);
			} else {
				std::cout << "Rehash all instead of bucket resize\n";
				rehashAll();
			}
			wasRehashed = true;
		}
		if (wasRehashed) {
			bucketIndex = bucketHashFunction(preHash);
			bucket<Key, T>& rehashedBucket = buckets[bucketIndex];
			bucket_entry<Key, T>& newEntry = rehashedBucket[preHash];
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(newEntry.getKey() == key);
			return newEntry.getValue();
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
		return buckets[bucketIndex].find(preHash, key);
    }

    size_t erase(const Key &key) override {
		size_t preHash = preHashFunction(std::move(key));
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket<Key, T>& bucket = buckets[bucketIndex];
		bucket_entry<Key, T>& entry = bucket[preHash];

		if (entry.isInitialized() and !entry.isDeleted()) {
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(entry.getKey() == key);
			entry.markDeleted();
			--_elementAmount;
			++count;
			++bucket.b;
			--bucket.elementAmount;
			return 1;
		}
		if (count >= M) {
			rehashAll();
		}
		return 0;
    }

    size_t size() const override {
		return _elementAmount;
	}

    void clear() override {
		M = calculateM(0);
		count = 0;
		bucketAmount = calculateBucketAmount(0);
		_elementAmount = 0;
		buckets.clear();
    	buckets.resize(bucketAmount, bucket<Key, T>());
	}

private:
	static size_t calculateM(size_t elementAmount) {
		return (1 + c) * std::max(elementAmount, size_t(4));
	}

	static size_t calculateBucketAmount(size_t elementAmount) {
		return std::max(size_t(10), elementAmount / 1500);
	}

	bool globalConditionIsSatisfied(size_t bucketLengthOfBucketToResize,
	                        		size_t bucketIndexOfBucketToResize) {
		size_t lengthSum = 0;
		for (size_t i = 0; i < bucketAmount; ++i) {
			if (i == bucketIndexOfBucketToResize) {
				lengthSum += bucketLengthOfBucketToResize;
			} else {
				lengthSum += buckets[i].length;
			}
		}
		return globalConditionIsSatisfied(lengthSum);
	}

	bool globalConditionIsSatisfied() {
		size_t lengthSum = 0;
		for (size_t i = 0; i < bucketAmount; ++i) {
			lengthSum += buckets[i].length;
		}
		return globalConditionIsSatisfied(lengthSum);
	}

	bool globalConditionIsSatisfied(size_t lengthSum) {
		return lengthSum <= ((32 * M * M) / bucketAmount) + 4 * M;
	}

	void rehashAll(const Key &key) {
		size_t preHash = preHashFunction(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket<Key, T>& keyBucket = buckets[bucketIndex];
		bucket_entry<Key, T>& entry = keyBucket[preHash];
		bool hadCollision = entry.getKey() != key;

		// TODO Use copy_if with move_iterator?
		// Collecting entries of the bucket
		std::vector<bucket_entry<Key, T>> entries(hadCollision ? _elementAmount + 1 : _elementAmount);
		size_t j = 0;
		for (size_t b = 0; b < buckets.size(); ++b) {
			bucket<Key, T>& bucket = buckets[b];
			std::vector<bucket_entry<Key, T>>& bucketEntries = bucket.getEntries();
			for (size_t i = 0; i < bucketEntries.size(); ++i) {
				bucket_entry<Key, T>& entry = bucketEntries[i];
				if (entry.isInitialized() && !entry.isDeleted()) {
					entries[j] = entry;
					++j;
				}
			}
		}
		if (hadCollision) {
			//there was a collision. append current inserted element
			bucket_entry<Key, T> entry = bucket_entry<Key, T>();
			entry.initialize(key);
			entries[j] = entry;
		}
		rehashAll(entries);
	}

	void rehashAll() {
		// Collecting entries of the bucket
		std::vector<bucket_entry<Key, T>> entries(_elementAmount);
		size_t j = 0;
		for (size_t b = 0; b < buckets.size(); ++b) {
			bucket<Key, T>& bucket = buckets[b];
			std::vector<bucket_entry<Key, T>>& bucketEntries = bucket.getEntries();
			for (size_t i = 0; i < bucketEntries.size(); ++i) {
				bucket_entry<Key, T>& entry = bucketEntries[i];
				if (entry.isInitialized() && !entry.isDeleted()) {
					entries[j] = entry;
					++j;
				}
			}
		}
		rehashAll(entries);
	}

	void rehashAll(std::vector<bucket_entry<Key, T>>& elements) {
		count = elements.size();
		M = calculateM(count);
		bucketAmount = calculateBucketAmount(M);

		std::vector<std::vector<bucket_entry<Key, T>>> bucketedEntries;
		size_t lengthSum = 0;
		do {
			std::cout << "rehashAll: Creating new bucket hash function" << "\n";

			size_t prime = primes(count);
			size_t random = randoms(1, prime - 1);
			size_t random2 = randoms(1, prime - 1);
			bucketHashFunction.setParameters(random, random2, prime, bucketAmount);

			//Initializing helper vectors
			bucketedEntries.clear();
			bucketedEntries.resize(bucketAmount, std::vector<bucket_entry<Key, T>>(elements.size() / bucketAmount));
			std::vector<size_t> bucketIndices(bucketAmount);

			//Collecting elements for the buckets with new bucket hash Function
			for (size_t i = 0; i < elements.size(); ++i) {
				bucket_entry<Key, T>& entry = elements[i];
				size_t preHash = preHashFunction(entry.getKey());
				size_t bucketIndex = bucketHashFunction(preHash);
				std::vector<bucket_entry<Key, T>>& entriesForBucket = bucketedEntries[bucketIndex];
				size_t& elementIndex = bucketIndices[bucketIndex];
				if (elementIndex < entriesForBucket.size()) {
					entriesForBucket[elementIndex] = entry;
					++elementIndex;
				} else {
					entriesForBucket.push_back(entry);
					++elementIndex;
				}
			}

			lengthSum = 0;
			for (size_t i = 0; i < bucketAmount; ++i) {
				bucketedEntries[i].resize(bucketIndices[i]);
				lengthSum += bucketedEntries[i].size();
			}
		} while (!globalConditionIsSatisfied(lengthSum));

		//Updating the buckets
		buckets.clear();
		buckets.resize(bucketAmount);
		for (size_t i = 0; i < bucketAmount; ++i) {
			std::vector<bucket_entry<Key, T>>& bucketEntries = bucketedEntries[i];
			buckets.push_back(bucket<Key, T>(bucketEntries));
		}
	}
};

}