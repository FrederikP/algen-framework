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
class bucket_2 {
private:
	size_t _capacityFactor;
	size_t _lengthFactor;
	size_t _maxRehashAttempts;
	size_t _rehashLengthFactor;

public:
	size_t capacity;
	size_t length;
	size_t elementAmount;

private:
	prime_generator primes;
	random_generator randoms;

	PreHashFcn preHashFunction;
	entry_hash_function hashFunction;
	std::vector<bucket_entry<Key, T>> entries;

public:
	bucket_2() : bucket_2(0) { }

	bucket_2(std::vector<bucket_entry<Key, T>> initialEntries,
		     size_t capacityFactor, size_t lengthFactor, size_t maxRehashAttempts, size_t rehashLengthFactor) :
	bucket_2(initialEntries.size(),
		     capacityFactor, lengthFactor, maxRehashAttempts, rehashLengthFactor)
	{
		elementAmount = initialEntries.size();
		insertAll(initialEntries);
	}

	bucket_2(size_t initialSize) : bucket_2(initialSize,
											2, 5, 5, 2) { }

	bucket_2(size_t initialSize,
		     size_t capacityFactor, size_t lengthFactor, size_t maxRehashAttempts, size_t rehashLengthFactor) :
		_capacityFactor(capacityFactor),
		_lengthFactor(lengthFactor),
		_maxRehashAttempts(maxRehashAttempts),
		_rehashLengthFactor(rehashLengthFactor),
		capacity(std::max(size_t(10), initialSize)),
		length(calculateLength(capacity)),
		elementAmount(0),
		entries(length)
	{
		size_t random = randoms(1, length - 1);
		size_t random2 = randoms(1, length - 1);
		hashFunction.setParameters(random, random2, length);
	}

	bucket_entry<Key, T>& operator[](size_t preHash) {
		size_t index = hashFunction(preHash);
		return entries[index];
	}

    virtual ~bucket_2() = default;

    std::vector<bucket_entry<Key, T>>& getEntries() {
    	return entries;
    }

    maybe<T> find(size_t preHash, const Key &key) const {
    	size_t index = hashFunction(preHash);
		return entries[index].find(key);
    }

    size_t size() const {
    	return elementAmount;
    }

	void resizeAndRehash(const Key& key) {
		capacity = elementAmount * _capacityFactor;
		length = calculateLength(capacity);
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
			bucketEntries[j].initialize(key);
			++elementAmount;
		} else {
			bucketEntries.pop_back();
		}

		entries.clear();
		entries.resize(length);
		insertAll(bucketEntries);
	}

	// TODO should be private
	size_t calculateLength(size_t bucketCapacity) {
		size_t minLength = _lengthFactor * bucketCapacity;
		return primes(minLength);
	}

private:
	void insertAll(std::vector<bucket_entry<Key, T>> bucketEntries) {
		// Choose a new injective hash function randomly
		size_t rehashAttempts = 0;
		bool isInjective;
		do {
//			std::cout << "rehashBucket: Creating new entry hash function" << "\n";
			++rehashAttempts;

			isInjective = true;
			size_t random = randoms(1, length - 1);
			size_t random2 = randoms(1, length - 1);
			hashFunction.setParameters(random, random2, length);

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

			if (rehashAttempts > _maxRehashAttempts) {
//				std::cout << "More than " << _maxRehashAttempts << " attempts to find a new hash function." << "\n";

				length *= _rehashLengthFactor;
				entries.clear();
				entries.resize(length);
				rehashAttempts = 0;
			}
		} while (!isInjective);

		// TODO Use move semantic instead of copying?
		// Inserting the entries in the table
		for(size_t i = 0; i < bucketEntries.size(); ++i) {
			bucket_entry<Key, T>& entry = bucketEntries[i];
			size_t preHash = preHashFunction(entry.getKey());
			size_t index = hashFunction(preHash);
			entries[index] = entry;
		}

	}
};

template <typename Key, typename T,
		  typename PreHashFcn = std::hash<Key>>
class DPH_with_buckets_2 : public hashtable<Key, T> { // DPH = Dynamic Perfect Hashing
private:
	size_t _capacityFactor;
	size_t _elementAmountPerBucket;

	size_t _bucketCapacityFactor;
	size_t _bucketLengthFactor;
	size_t _bucketMaxRehashAttempts;
	size_t _bucketRehashLengthFactor;

	size_t capacity;
	size_t bucketAmount;

	prime_generator primes;
	random_generator randoms;
	
	PreHashFcn preHashFunction;
	bucket_hash_function bucketHashFunction;
	std::vector<bucket_2<Key, T>> buckets;
	
public:
    virtual ~DPH_with_buckets_2() = default;

    // Register all contenders in the list
    static void register_contenders(common::contender_list<hashtable<Key, T>> &list) {
        using Factory = common::contender_factory<hashtable<Key, T>>;
        list.register_contender(Factory("DPH-with-buckets-2", "DPH-with-buckets-2",
            [](){
				return new DPH_with_buckets_2(1000);
			}
        ));
//        list.register_contender(Factory("DPH-with-buckets-2-2", "DPH-with-buckets-2-2",
//            [](){
//				return new DPH_with_buckets_2(1000, 2, 5, 5, 2, 2, 3000);
//			}
//        ));
//        list.register_contender(Factory("DPH-with-buckets-2-3", "DPH-with-buckets-2-3",
//            [](){
//				return new DPH_with_buckets_2(1000, 2, 5, 5, 2, 3, 3000);
//			}
//        ));
//        list.register_contender(Factory("DPH-with-buckets-2-4", "DPH-with-buckets-2-4",
//            [](){
//				return new DPH_with_buckets_2(1000, 2, 5, 5, 2, 4, 3000);
//			}
//        ));
    }
	
    DPH_with_buckets_2(size_t initialElementAmount) : DPH_with_buckets_2(initialElementAmount,
    																	 2, 5, 5, 2,
																	     2, 3000) { }

    DPH_with_buckets_2(size_t initialElementAmount,
    				   size_t bucketCapacityFactor, size_t bucketLengthFactor, size_t bucketMaxRehashAttempts, size_t bucketRehashLengthFactor,
					   size_t tableCapacityFactor, size_t elementAmountPerBucket) :
    	hashtable<Key, T>(),
		_capacityFactor(tableCapacityFactor),
		_elementAmountPerBucket(elementAmountPerBucket),

		_bucketCapacityFactor(bucketCapacityFactor),
		_bucketLengthFactor(bucketLengthFactor),
		_bucketMaxRehashAttempts(bucketMaxRehashAttempts),
		_bucketRehashLengthFactor(bucketRehashLengthFactor),

		capacity(initialElementAmount),
		bucketAmount(calculateBucketAmount(initialElementAmount)),
		primes(),
		randoms(),
    	buckets(bucketAmount, bucket_2<Key, T>(_elementAmountPerBucket,
    										   _bucketCapacityFactor, _bucketLengthFactor, _bucketMaxRehashAttempts, _bucketRehashLengthFactor))
	{
		size_t prime = primes(bucketAmount);
		size_t random = randoms(1, prime - 1);
		size_t random2 = randoms(1, prime - 1);
		bucketHashFunction.setParameters(random, random2, prime, bucketAmount);
    }
		
    T& operator[](const Key &key) override {
		size_t preHash = preHashFunction(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket_2<Key, T>& _bucket = buckets[bucketIndex];
		bucket_entry<Key, T>& entry = _bucket[preHash];
		if (!entry.isInitialized()) {
			entry.initialize(key);
			++_bucket.elementAmount;
		} else if (entry.isDeleted()) {
			entry = bucket_entry<Key, T>();
			entry.initialize(key);
			++_bucket.elementAmount;
		}
		bool wasRehashed = false;
		if (size() > capacity || _bucket.elementAmount > _elementAmountPerBucket) {
//			std::cout << "Rehash all\n";
			rehashAll(key);
			wasRehashed = true;
		} else if (_bucket.elementAmount <= _bucket.capacity and entry.getKey() != key) {
//			std::cout << "Rehashing bucket\n";
			_bucket.rehash(key);
			wasRehashed= true;
		} else if (_bucket.elementAmount > _bucket.capacity or _bucket.elementAmount <= _bucket.capacity / (_bucketCapacityFactor * 2)) {
			//TODO Code duplication
			size_t newBucketCapacity = _bucket.elementAmount * _bucketCapacityFactor;
			size_t newBucketLength = _bucket.calculateLength(newBucketCapacity);
			if (globalConditionIsSatisfied(newBucketLength, bucketIndex)) {
	//			std::cout << "Resizing bucket: elementAmount=" << _bucket.elementAmount << "  oldLength= " << _bucket.length << "\n";
				_bucket.resizeAndRehash(key);
//				std::cout << " newLength= " << _bucket.length << "\n";
			} else {
//				std::cout << "Rehash all instead of bucket resize\n";
				rehashAll();
			}
			wasRehashed = true;
		}
		if (wasRehashed) {
			bucketIndex = bucketHashFunction(preHash);
			bucket_2<Key, T>& rehashedBucket = buckets[bucketIndex];
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
		bucket_2<Key, T>& bucket = buckets[bucketIndex];
		bucket_entry<Key, T>& entry = bucket[preHash];

		if (entry.isInitialized() and !entry.isDeleted()) {
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(entry.getKey() == key);
			entry.markDeleted();
			--bucket.elementAmount;
			return 1;
		}
		if (size() < capacity / (_capacityFactor * 2)) {
			rehashAll();
		}
		return 0;
    }

    size_t size() const override {
    	size_t _size = 0;
    	for (size_t i = 0; i < buckets.size(); ++i) {
    		_size += buckets[i].size();
    	}
		return _size;
	}

    void clear() override {
		capacity = size() * _capacityFactor;
		bucketAmount = calculateBucketAmount(0);
		buckets.clear();
    	buckets.resize(bucketAmount, bucket_2<Key, T>(0, _bucketCapacityFactor, _bucketLengthFactor, _bucketMaxRehashAttempts, _bucketRehashLengthFactor));
	}

private:
	size_t calculateBucketAmount(size_t elementAmount) {
		return std::max(size_t(10), elementAmount / _elementAmountPerBucket);
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
		return lengthSum <= ((32 * capacity * capacity) / bucketAmount) + 4 * capacity;
	}

	void rehashAll(const Key &key) {
		size_t preHash = preHashFunction(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket_2<Key, T>& keyBucket = buckets[bucketIndex];
		bucket_entry<Key, T>& entry = keyBucket[preHash];
		bool hadCollision = entry.getKey() != key;

		// TODO Use copy_if with move_iterator?
		// Collecting entries of the bucket
		std::vector<bucket_entry<Key, T>> entries(hadCollision ? size() + 1 : size());
		size_t j = 0;
		for (size_t b = 0; b < buckets.size(); ++b) {
			bucket_2<Key, T>& bucket = buckets[b];
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
		std::vector<bucket_entry<Key, T>> entries(size());
		size_t j = 0;
		for (size_t b = 0; b < buckets.size(); ++b) {
			bucket_2<Key, T>& bucket = buckets[b];
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
		capacity = elements.size() * _capacityFactor;
		bucketAmount = calculateBucketAmount(capacity);

		std::vector<std::vector<bucket_entry<Key, T>>> bucketedEntries;
		size_t lengthSum = 0;
		do {
//			std::cout << "rehashAll: Creating new bucket hash function" << "\n";

			size_t prime = primes(bucketAmount);
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
			buckets[i] = bucket_2<Key, T>(bucketEntries,
					 	 	 	 	 	_bucketCapacityFactor, _bucketLengthFactor, _bucketMaxRehashAttempts, _bucketRehashLengthFactor);
		}
	}
};

}
