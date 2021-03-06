#pragma once

#include <cassert>

#include <sparsehash/sparse_hash_map>

#include "../common/contenders.h"
#include "hashtable.h"

namespace hashtable {

template <typename Key,
          typename T,
          typename HashFcn = std::hash<Key>,
          typename EqualKey = std::equal_to<Key>,
          typename Alloc = google::libc_allocator_with_realloc<std::pair<const Key, T>>>
class sparse_hash_map : public hashtable<Key, T> {
public:
    sparse_hash_map(const size_t bucket_count = 0, const Key deleted_key = Key{-1}) : hashtable<Key, T>(), map(bucket_count) {
        map.set_deleted_key(deleted_key);
    }
    virtual ~sparse_hash_map() = default;

    // Register all contenders in the list
    static void register_contenders(common::contender_list<hashtable<Key, T>> &list) {
        using Factory = common::contender_factory<hashtable<Key, T>>;
        list.register_contender(Factory("sparse_hash_map", "sparse-hash-map",
            [](){ return new sparse_hash_map<Key, T>(); }
        ));
        //list.register_contender(Factory("sparse_hash_map with std::allocator", "sparse_hash_map std_allocator",
        //    [](){ return new sparse_hash_map<Key, T, std::hash<Key>, std::equal_to<Key>, std::allocator<std::pair<const Key, T>>>(); }
        //));
    }

    T& operator[](const Key &key) override {
        return map[key];
    }

    T& operator[](Key&& key) override {
        return map[std::move(key)];
    }

    maybe<T> find(const Key &key) const override {
        auto it = map.find(key);
        if (it == map.end()) {
            return nothing<T>();
        } else {
            assert(it->first == key);
            return just<T>(it->second);
        }
    }

    size_t erase(const Key &key) override {
        return map.erase(key);
    }

    size_t size() const override { return map.size(); }

    void clear() override { map.clear(); }

protected:
    google::sparse_hash_map<Key, T, HashFcn, EqualKey, Alloc> map;
};

}
