// Copyright 2023 The Cross-Media Measurement Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_MAIN_CC_WFA_VIRTUAL_PEOPLE_CORE_SELECTOR_LRU_CACHE_H_
#define SRC_MAIN_CC_WFA_VIRTUAL_PEOPLE_CORE_SELECTOR_LRU_CACHE_H_

#include <ctime>
#include <list>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/farmhash.h"

namespace wfa_virtual_people {

struct ModelReleasePercentile {
  double end_percentile;
  std::string model_release_resource_key;
};

struct TmHash {
    std::size_t operator()(const std::tm& tm) const {
        std::size_t seed = 0;
        seed ^= std::hash<int>{}(tm.tm_year) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(tm.tm_mon) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(tm.tm_mday) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct TmEqual {
    bool operator()(const std::tm& lhs, const std::tm& rhs) const {
        return lhs.tm_year == rhs.tm_year &&
               lhs.tm_mon == rhs.tm_mon &&
               lhs.tm_mday == rhs.tm_mday;
    }
};

// TODO(@marcopremier): Move this class in common-cpp
// Definition of a least recently used (LRU) cache with a fixed maximum number of elements.
class LruCache {
 public:
   explicit LruCache(int max_elements);

   // Add a new entry into the cache. If the cache is full, the oldest element is
   // removed.
   void Add(const std::tm& key, const std::vector<ModelReleasePercentile>& data);

   // Returns an element by its key, or nullopt if the key is not found.
   std::optional<std::vector<ModelReleasePercentile>> Get(const std::tm& key);

  private:
   absl::flat_hash_map<std::tm, std::vector<ModelReleasePercentile>, TmHash, TmEqual>
       cache_data;
   std::list<std::tm> access_order;
   int cache_size;
};

}  // namespace wfa_virtual_people

#endif  // SRC_MAIN_CC_WFA_VIRTUAL_PEOPLE_CORE_SELECTOR_LRU_CACHE_H_
