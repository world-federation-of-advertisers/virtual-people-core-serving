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

#include "absl/container/flat_hash_map.h"
#include "src/farmhash.h"

namespace wfa_virtual_people {

struct ModelReleasePercentile {
  double end_percentile;
  std::string model_release_resource_key;
};

class LruCache {
 public:
  LruCache(int n);

  void Add(const std::tm& key, const std::vector<ModelReleasePercentile>& data);

  std::optional<std::vector<ModelReleasePercentile>> Get(const std::tm& key);

 private:
  absl::flat_hash_map<std::string, std::vector<ModelReleasePercentile>>
      cache_data;
  std::list<std::string> access_order;
  int cache_size;

  std::string TmToString(const std::tm& tm);
};

}  // namespace wfa_virtual_people

#endif  // SRC_MAIN_CC_WFA_VIRTUAL_PEOPLE_CORE_SELECTOR_LRU_CACHE_H_
