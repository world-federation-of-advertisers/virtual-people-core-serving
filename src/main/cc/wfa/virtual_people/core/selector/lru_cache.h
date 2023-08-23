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

#include "absl/container/flat_hash_map.h"
#include "src/farmhash.h"

#include <ctime>

namespace wfa_virtual_people {

struct ModelReleasePercentile {
  double end_percentile;
  std::string model_release_resource_key;
};

struct TmComparator {
  bool operator()(const std::tm& left_date, const std::tm& right_date) const {
    return std::tie(left_date.tm_year, left_date.tm_mon, left_date.tm_mday) <
           std::tie(right_date.tm_year, right_date.tm_mon, right_date.tm_mday);
  }
};

struct TmHash {
  std::size_t operator()(const std::tm& tm) const {
    return util::Hash128to64(util::Uint128(
                               static_cast<uint64_t>(tm.tm_year),
                               static_cast<uint64_t>(tm.tm_mon) << 32 | static_cast<uint64_t>(tm.tm_mday) << 32
                             ));
  }
};

class LruCache {
 public:
  LruCache(int n);

  void Add(const std::tm& key, const std::vector<ModelReleasePercentile>& data);

  std::optional<std::vector<ModelReleasePercentile>> Get(const std::tm& key);

 private:
  absl::flat_hash_map<std::tm, std::vector<ModelReleasePercentile>, TmHash, TmComparator> cache_data;
  std::list<std::tm> access_order;
  int cache_size;
};

}  // namespace wfa_virtual_people

#endif  // SRC_MAIN_CC_WFA_VIRTUAL_PEOPLE_CORE_SELECTOR_LRU_CACHE_H_
