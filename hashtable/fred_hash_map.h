#pragma once

#include <cassert>

#include "../common/contenders.h"
#include "hashtable.h"
#include <primesieve.hpp>

namespace hashtable {

class outer_universal_hash_fcn {
public:	
	outer_universal_hash_fcn(size_t M, size_t numberOfSubBlocks) {
		std::vector<size_t> primes;
		// Choose first prime >= M
		primesieve::generate_n_primes<size_t>(1, M, &primes);
		p = primes[0];
		s = numberOfSubBlocks;
		// Seed with a real random value, if available
		std::random_device rd;
 
		// Choose a random factor k between 1 and p - 1
		std::default_random_engine e1(rd());
		std::uniform_int_distribution<size_t> uniform_dist(1, p - 1);
		k = uniform_dist(e1);
	}
	size_t operator()(size_t& x) const {
		return (k * x % p) % s;
	}
private:
	size_t k;
	size_t p;
	size_t s;
};

class inner_universal_hash_fcn {
public:	
	inner_universal_hash_fcn(size_t size) {
		std::vector<size_t> primes;
		// Choose first prime >= M
		primesieve::generate_n_primes<size_t>(1, size, &primes);
		p = primes[0];
		// Seed with a real random value, if available
		std::random_device rd;
 
		// Choose a random factor k between 1 and p - 1
		std::default_random_engine e1(rd());
		std::uniform_int_distribution<size_t> uniform_dist(1, p - 1);
		k = uniform_dist(e1);
	}
	size_t operator()(size_t& x) const {
		return k * x % p;
	}
private:
	size_t k;
	size_t p;
};

template <typename Key,
          typename T>
class inner_table_entry{
public:
	inner_table_entry(Key& elementKey, T& elementValue) : key(elementKey), t(elementValue) {
	}
	inner_table_entry() : key(0), t(0) {}
	Key& getKey() {
		return key;
	}
	T& getValue() {
		return t;
	}
private:
	Key key;
	T t;
};

template <typename Key,
          typename T>
class outer_table_entry {
public:
	outer_table_entry() : innerHashFcn(computeSetAndReturnMl(0)), innerTable(ml) {
		
	}
	size_t computeSetAndReturnMl(size_t initialElementCount) {
		size_t minimumSize = 10;
		size_t max = std::max(initialElementCount, minimumSize);
		ml = max * (max - 1) + 1;
		return ml;
	}
	T& getValue(size_t key) {
		size_t innerIndex = innerHashFcn(key);
		inner_table_entry<Key, T>& entry = innerTable[innerIndex];
		return entry.getValue();
	}
private:
	size_t ml;
	inner_universal_hash_fcn innerHashFcn;
	std::vector< inner_table_entry<Key, T> > innerTable;
};

template <typename Key,
          typename T,
	      typename PreHashFcn = std::hash<Key>,
		  typename OuterHashFcn = outer_universal_hash_fcn>
class fred_hash_map : public hashtable<Key, T> {
public:
    fred_hash_map(size_t initialM, size_t numberOfSubBlocks) : hashtable<Key, T>(), outerHashFcn(initialM, numberOfSubBlocks), outerTable(s) {
		M = initialM;
		count = 0;
		s = numberOfSubBlocks;
		for (size_t i = 0; i < s; i++) {
			outerTable[i] = outer_table_entry<Key,T>();
		}
    }
    virtual ~fred_hash_map() = default;

    // Register all contenders in the list
    static void register_contenders(common::contender_list<hashtable<Key, T>> &list) {
        using Factory = common::contender_factory<hashtable<Key, T>>;
		size_t initialM = 10;
		size_t s = 100;
        list.register_contender(Factory("fred_hash_map", "fred-hash-map",
            [initialM, s](){ return new fred_hash_map<Key, T>(initialM, s); }
        ));
    }
		
    T& operator[](const Key &key) override {
        size_t preHash = preHashFcn(key);
		size_t subTableIndex = outerHashFcn(preHash);
		outer_table_entry< Key, T > outerEntry = outerTable[subTableIndex];
		return outerEntry.getValue(preHash);
	}

    T& operator[](Key&& key) override {
        size_t preHash = preHashFcn(std::move(key));
		size_t subTableIndex = outerHashFcn(preHash);
		outer_table_entry< Key, T > outerEntry = outerTable[subTableIndex];
		return outerEntry.getValue(preHash);
    }

    maybe<T> find(const Key &key) const override {
        size_t preHash = preHashFcn(key);
		size_t subTableIndex = outerHashFcn(preHash);
		outer_table_entry< Key, T > outerEntry = outerTable[subTableIndex];
		return just<T>(outerEntry.getValue(preHash));
		//return nothing<T>();
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

private:
	PreHashFcn preHashFcn;
	OuterHashFcn outerHashFcn;
	std::vector< outer_table_entry < Key, T > > outerTable;
	size_t M;
	size_t count;
	size_t s;
};

}
