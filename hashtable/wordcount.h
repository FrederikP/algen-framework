#pragma once

#include <cassert>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <string>
#include <map>

#include "../common/benchmark.h"
#include "../common/contenders.h"

namespace hashtable {
    
static const std::map < size_t, std::pair< std::string, std::string > >  fileNameMap = {
    {0, std::make_pair("Kafka", "Verwandl")},
    {1, std::make_pair("Shakesp", "complete")}
};

template <typename HashTable>
class wordcount {
public:
    using Configuration = std::pair<size_t, size_t>;
    using Benchmark = common::benchmark<HashTable, Configuration>;
    using BenchmarkFactory = common::contender_factory<Benchmark>;
    
    // fake word count, doesn't actually determine the most frequent
    // words because our hashtables don't have an iterator interface
    template <typename It>
    static void count(HashTable &map, It begin, It end) {
        It it = begin;
        while (it != end) {
            map[*it++]++;
        }
    }

    static void register_benchmarks(common::contender_list<Benchmark> &benchmarks) {
        // HACKHACKHACK
        const std::vector<Configuration> configs{
            std::make_pair(0, 0), // "Kafka", "Verwandl"
            std::make_pair(1, 0) // "Shakesp", "complete"
        };

        using Key = typename HashTable::key_type;

        common::register_benchmark("wordcount", "wordcount",
            [](HashTable&, Configuration config, void*) -> void* {
                size_t nameIndex = config.first;
                std::pair< std::string, std::string > namePair = fileNameMap[nameIndex];
                // awful hack approaching
                std::stringstream fn;
                // convert encoded filename back to ascii
                fn << "data/wordcount_" << namePair.first;
                if (!namePair.second.empty())
                    fn << "_" << namePair.second;
                fn << ".txt";

                std::ifstream in(fn.str());
                if (!in.is_open())
                    throw std::invalid_argument("Cannot open file '" + fn.str() + "'.");

                // map strings to key type because stupid benchmark
                std::unordered_map<std::string, Key> ids;
                ids[""] = Key{}; // dummy to use key Key{}
                auto words = new std::vector<Key>();

                std::string word;
                while (in >> word) {
                    Key& key = ids[word];
                    if (key == Key{}) {
                        key = static_cast<Key>(ids.size());
                    }
                    words->push_back(key);
                }

                // Allocate data array and copy
                return words;
            },
            [](HashTable &map, Configuration, void* ptr) {
                assert(ptr != nullptr);
                auto data = static_cast<std::vector<Key>*>(ptr);
                wordcount::count(map, data->begin(), data->end());
            },
            [](HashTable &, Configuration, void* ptr) {
                delete static_cast<std::vector<Key>*>(ptr);
            }, configs, benchmarks);
    }

};

}
