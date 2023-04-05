#pragma once

#include "log_duration.h"

#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        std::unique_lock<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count)
        :buckets(bucket_count) {}

    Access operator[](const Key& key) {
        const uint64_t index = static_cast<uint64_t>(key) % buckets.size();
        Bucket& bucket_ref = buckets[index];

        return { std::unique_lock(bucket_ref.mutex), bucket_ref.bucket[key]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {

        std::map<Key, Value> result;

        for (Bucket& b : buckets) {
            std::lock_guard guard(b.mutex);
            for (const auto& [key, value] : b.bucket) {
                result[key] = value;
            }
        }

        return result;
    }

private:

    struct Bucket
    {
        std::map<Key, Value> bucket;
        std::mutex mutex;
    };

    std::vector<Bucket> buckets;
};
