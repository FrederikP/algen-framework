#pragma once

#include <cassert>
#include <iostream>

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
	size_t elementAmount;
	hash_function hashFunction;

	bucket_info() { }

	bucket_info(size_t bucketM, size_t bucketStart, size_t bucketLength, size_t random,  size_t random2, size_t prime) {
		M = bucketM;
		b = 0;
		start = bucketStart;
		length = bucketLength;
		elementAmount = 0;
		hashFunction.setParameters(random, random2, prime, length);
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
	size_t count;
	
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
				return new DPH_with_single_vector(initialElementAmount); 
			}
        ));
    }
	
    DPH_with_single_vector(size_t initialElementAmount) : hashtable<Key, T>() {
		c = 5;
		M = calculateM(initialElementAmount);
		count = 0;
		_elementAmount = 0;
		bucketAmount = calculateBucketAmount(initialElementAmount);

		size_t prime = primes(initialElementAmount);
		size_t random = randoms(1, prime - 1);
		size_t random2 = randoms(1, prime - 1);
		bucketHashFunction.setParameters(random, random2, prime, bucketAmount);

		bucketInfos = std::vector<bucket_info>(bucketAmount);
		size_t initialElementPerBucketAmount = initialElementAmount / bucketAmount;
		size_t bucketM = std::max(size_t(10), initialElementPerBucketAmount);
		size_t bucketLength = calculateBucketLength(bucketM);
		size_t bucketPrime = primes(bucketLength);
		for(size_t i = 0; i < bucketAmount; i++) {
			size_t bucketStart = i == 0 ? 0 : i * bucketLength;
			size_t random = randoms(1, bucketPrime - 1);
			size_t random2 = randoms(1, bucketPrime - 1);

			bucketInfos[i] = bucket_info(bucketM, bucketStart, bucketLength, random, random2, bucketPrime);
		}

		entries = std::vector< bucket_entry< Key, T > >(bucketAmount * bucketLength);
    }
		
    T& operator[](const Key &key) override {
		size_t preHash = preHashFunction(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket_info& bucket = bucketInfos[bucketIndex];
		size_t elementIndex = bucket.index(preHash);
		bucket_entry<Key, T>& entry = entries[elementIndex];
		if (!entry.isInitialized()) {
			entry.initialize(key);
			_elementAmount++;
			count++;
			bucket.b++;
			bucket.elementAmount++;
		} else if (entry.isDeleted()) {
			entry = bucket_entry<Key, T>();
			entry.initialize(key);
			_elementAmount++;
			count++;
			bucket.b++;
			bucket.elementAmount++;
		}
		bool wasRehashed = false;
		if (count >= M) {
			std::cout << "Rehash all count>=M: " << count << ">="<< M << "\n";
			rehashAll(key);
			wasRehashed = true;
		} else if (bucket.b <= bucket.M and entry.getKey() != key) {
			std::cout << "Rehashing bucket bucket.b <= bucket.M: " << bucket.b << "<="<< bucket.M << "\n";
			rehashBucket(bucket, key);
			wasRehashed= true;
		} else if (bucket.b > bucket.M) {
			size_t newBucketM = bucket.M * 2;
			size_t newBucketLength = calculateBucketLength(newBucketM);
			size_t lengthAddition = newBucketLength - bucket.length;
			if (globalConditionIsSatisfied(newBucketLength, bucketIndex)) {
				std::cout << "Resizing bucket bucket.b > bucket.M: " << bucket.b << ">"<< bucket.M << "\n";
				// Create new main vector
				size_t newVectorLength = entries.size() + lengthAddition;
				std::vector< bucket_entry< Key, T > > newEntriesVector = std::vector< bucket_entry< Key, T > >(newVectorLength);
				std::copy(entries.begin(),
						entries.begin() + bucket.start + bucket.length,
						newEntriesVector.begin());
				std::copy(
						entries.begin() + bucket.start
								+ bucket.length + 1, entries.end(),
						newEntriesVector.begin() + bucket.start
								+ newBucketLength + 1);

				for (size_t i = bucketIndex + 1; i < bucketInfos.size(); i++) {
					bucket_info& info = bucketInfos[i];
					info.start = info.start + lengthAddition;
				}
				bucket.length = newBucketLength;
				bucket.M = newBucketM;
				entries = newEntriesVector;
				rehashBucket(bucket, key);
			} else {
				std::cout << "Rehash all instead of bucket resize\n";
				rehashAll();
			}
			wasRehashed = true;
		}
		if (wasRehashed) {
			bucketIndex = bucketHashFunction(preHash);
			bucket = bucketInfos[bucketIndex];
			elementIndex = bucket.index(preHash);
			bucket_entry<Key, T>& newEntry = entries[elementIndex];
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
		bucket_info& bucket = bucketInfos[bucketIndex];
		size_t elementIndex = bucket.index(preHash);
		bucket_entry<Key, T>& entry = entries[elementIndex];

		if (entry.isInitialized() and !entry.isDeleted()) {
			// If this is not the case something with the dynamic rehashing didn't work out
			assert(entry.getKey() == key);
			entry.markDeleted();
			_elementAmount--;
			count++;
			bucket.b++;
			bucket.elementAmount--;
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
		size_t entriesCapacity = entries.capacity();
		entries = std::vector< bucket_entry< Key, T > >(entriesCapacity);
		_elementAmount = 0;
	}

private:
	size_t calculateM(size_t elementAmount) {
		return (1 + c) * std::max(elementAmount, size_t(4));
	}

	size_t calculateBucketAmount(size_t elementAmount) {
		return std::max(size_t(10), elementAmount / 1000);
	}

	size_t calculateBucketLength(size_t bucketM) {
		return bucketM * (bucketM - 1);
	}

	bool globalConditionIsSatisfied(size_t bucketLengthOfBucketToResize,
	                        		size_t bucketIndexOfBucketToResize) {
		size_t lengthSum = 0;
		for (size_t i = 0; i < bucketAmount; i++) {
			if (i == bucketIndexOfBucketToResize) {
				lengthSum += bucketLengthOfBucketToResize;
			} else {
				lengthSum += bucketInfos[i].length;
			}
		}
		return lengthSum <= ((32 * M * M) / bucketAmount) + 4 * M;
	}

	bool globalConditionIsSatisfied() {
		size_t lengthSum = 0;
		for (size_t i = 0; i < bucketAmount; i++) {
			lengthSum += bucketInfos[i].length;
		}
		return lengthSum <= ((32 * M * M) / bucketAmount) + 4 * M;
	}

	void rehashBucket(bucket_info& bucket, const Key& key) {
		//std::cout << "Rehash da bucket!\n";

		// Collecting entries of the bucket
		std::vector<bucket_entry<Key, T>> bucketEntries = std::vector<bucket_entry<Key, T>>(bucket.elementAmount + 1);
		size_t j = 0;
		bool includesNewKey = false;
		for (size_t i = bucket.start; i < bucket.start + bucket.length; i++) {
			bucket_entry<Key, T> entry = entries[i];
			if (entry.isInitialized() && !entry.isDeleted()) {
				if (entry.getKey() == key) {
					includesNewKey = true;
				}
				bucketEntries[j] = entry;
				j++;
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

		// Choose a new injective hash function randomly
		bool isInjective;
		do {
			isInjective = true;
			size_t prime = primes(bucket.length);
			size_t random = randoms(1, prime - 1);
			size_t random2 = randoms(1, prime - 1);
			bucket.hashFunction.setParameters(random, random2, prime, bucket.length);

			size_t lastPreHash = preHashFunction(key);
			size_t lastIndex = bucket.hashFunction(lastPreHash);
			((void) lastIndex);

			std::vector<size_t> indices = std::vector<size_t>(bucket.length);
			for(size_t i = 0; i < bucketEntries.size(); i++) {
				bucket_entry<Key, T>& entry = bucketEntries[i];
				if (entry.isInitialized()) {
					size_t preHash = preHashFunction(entry.getKey());
					size_t index = bucket.hashFunction(preHash);
					if (indices[index] != 0) {
						isInjective = false;
						break;
					} else {
						indices[index] = 1;
					}
				}
			}
		} while (!isInjective);
		// Inserting the entries in the table
		for(size_t i = 0; i < bucketEntries.size(); i++) {
			bucket_entry<Key, T> entry = bucketEntries[i];
			size_t preHash = preHashFunction(entry.getKey());
			size_t index = bucket.index(preHash);
			entries[index] = entry;
		}
	}

	void rehashAll(const Key &key) {
		bool hadCollision = false;
		size_t preHash = preHashFunction(key);
		size_t bucketIndex = bucketHashFunction(preHash);
		bucket_info& bucket = bucketInfos[bucketIndex];
		size_t elementIndex = bucket.index(preHash);
		bucket_entry<Key, T>& existingEntry = entries[elementIndex];
		if (existingEntry.getKey() != key) {
			hadCollision = true;
		}

		// Collecting entries of the bucket
		std::vector<bucket_entry<Key, T>> bucketEntries = std::vector<bucket_entry<Key, T>>(hadCollision ? _elementAmount + 1 : _elementAmount);
		size_t j = 0;
		for (size_t i = 0; i < entries.size(); i++) {
			bucket_entry<Key, T> entry = entries[i];
			if (entry.isInitialized() && !entry.isDeleted()) {
				bucketEntries[j] = entry;
				j++;
			}
		}
		if (hadCollision) {
			//there was a collision. append current inserted element
			bucket_entry<Key, T> entry = bucket_entry<Key, T>();
			entry.initialize(key);
			bucketEntries[j] = entry;
		}
		rehashAll(bucketEntries);
	}

	void rehashAll() {
		// Collecting entries of the bucket
		std::vector<bucket_entry<Key, T>> bucketEntries = std::vector<bucket_entry<Key, T>>(_elementAmount);
		size_t j = 0;
		for (size_t i = 0; i < entries.size(); i++) {
			bucket_entry<Key, T> entry = entries[i];
			if (entry.isInitialized() && !entry.isDeleted()) {
				bucketEntries[j] = entry;
				j++;
			}
		}
		rehashAll(bucketEntries);
	}

	void rehashAll(std::vector<bucket_entry<Key, T>> bucketEntries) {
		//std::cout << "I trya rehorst everything!\n";

		count = bucketEntries.size();
		M = calculateM(count);
		bucketAmount = calculateBucketAmount(M);

		size_t lengthSum;
		std::vector<std::vector<bucket_entry<Key, T>>> bucketedEntries;
		do {
			lengthSum = 0;
			size_t prime = primes(count);
			size_t random = randoms(1, prime - 1);
			size_t random2 = randoms(1, prime - 1);
			bucketHashFunction.setParameters(random, random2, prime, bucketAmount);

			bucketedEntries = std::vector<std::vector<bucket_entry<Key, T>>>(bucketAmount);
			for (size_t i = 0; i < bucketEntries.size(); i++) {
				bucket_entry<Key, T> entry = bucketEntries[i];
				size_t preHash = preHashFunction(entry.getKey());
				size_t bucketIndex = bucketHashFunction(preHash);
				bucketedEntries[bucketIndex].push_back(entry);
			}

			bucketInfos = std::vector<bucket_info>(bucketAmount);
			for (size_t i = 0; i < bucketInfos.size(); i++) {
				bucket_info& bucket = bucketInfos[i];
				bucket.elementAmount = bucketedEntries[i].size();
				if (i == 0) {
					bucket.start = 0;
				} else {
					bucket.start = bucketInfos[i-1].start + bucketInfos[i-1].length;
				}
				bucket.b = bucketedEntries[i].size();
				bucket.M = 2 * bucket.b;
				bucket.length = calculateBucketLength(bucket.M);
				lengthSum += bucket.length;
			}
		} while (!globalConditionIsSatisfied());

		entries = std::vector< bucket_entry< Key, T > >(lengthSum);

		for (size_t bucketIndex = 0; bucketIndex < bucketInfos.size(); bucketIndex++) {
			bucket_info& bucket = bucketInfos[bucketIndex];
			std::vector<bucket_entry<Key, T>>& entriesForBucket = bucketedEntries[bucketIndex];
			// Choose a new injective hash function randomly
			bool isInjective;
			do {
				isInjective = true;
				size_t prime = primes(bucket.length);
				size_t random = randoms(1, prime - 1);
				size_t random2 = randoms(1, prime - 1);
				bucket.hashFunction.setParameters(random, random2, prime, bucket.length);
				std::vector<size_t> indices = std::vector<size_t>(bucket.length);
				for(size_t i = 0; i < entriesForBucket.size(); i++) {
					bucket_entry<Key, T>& entry = entriesForBucket[i];
					size_t preHash = preHashFunction(entry.getKey());
					size_t index = bucket.hashFunction(preHash);
					if (indices[index] != 0) {
						isInjective = false;
						break;
					} else {
						indices[index] = 1;
					}
				}
			} while (!isInjective);
			// Inserting the entries in the table
			for(size_t i = 0; i < entriesForBucket.size(); i++) {
				bucket_entry<Key, T> entry = entriesForBucket[i];
				size_t preHash = preHashFunction(entry.getKey());
				size_t index = bucket.index(preHash);
				entries[index] = entry;
			}
		}
	}
};

}
