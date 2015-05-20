#pragma once

#include <cassert>
#include <unordered_map>

#include "hashtable.h"

namespace hashtable {
    
template <typename Key,
          typename T,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<std::pair<const Key, T>>>
class unordered_map : public hashtable<Key, T> {
public:
    unordered_map(const size_t bucket_count = 0) : hashtable<Key, T>(), map(bucket_count) {}
    ~unordered_map() {}

    T& operator[](const Key &key) {
        return map[key];
    }

    T& operator[](Key&& key) {
        return map[std::forward<Key>(key)];
    }

    maybe<T> find(const Key &key) {
        auto it = map.find(key);
        if (it == map.end()) {
            return nothing<T>();
        } else {
            assert(it->first == key);
            return just<T>(it->second);
        }
    }

    size_t erase(const Key &key) {
        return map.erase(key);
    }

    size_t size() const { return map.size(); }

    void clear() { map.clear(); }

protected:
    std::unordered_map<Key, T, Hash, KeyEqual, Allocator> map;
};

};