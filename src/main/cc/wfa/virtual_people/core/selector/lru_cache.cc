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

#include "wfa/virtual_people/core/selector/lru_cache.h"

namespace wfa_virtual_people {

LruCache::LruCache(int n) : cache_size(n) {}

std::string LruCache::TmToString(const std::tm& tm) {
  return std::to_string(tm.tm_year) + "-" + std::to_string(tm.tm_mon) + "-" +
         std::to_string(tm.tm_mday);
}

void LruCache::Add(const std::tm& key,
                   const std::vector<ModelReleasePercentile>& data) {
  if (cache_data.size() >= cache_size) {
    auto oldest = access_order.begin();
    for (auto idx = access_order.begin(); idx != access_order.end(); ++idx) {
      if (*idx < *oldest) {
        oldest = idx;
      }
    }
    cache_data.erase(*oldest);
    access_order.erase(oldest);
  }
  cache_data[TmToString(key)] = data;
  access_order.emplace_front(TmToString(key));
}

std::optional<std::vector<ModelReleasePercentile>> LruCache::Get(
    const std::tm& key) {
  auto idx = cache_data.find(TmToString(key));
  if (idx != cache_data.end()) {
    for (auto it = access_order.begin(); it != access_order.end(); ++it) {
      if (*it == TmToString(key)) {
        access_order.erase(it);
        access_order.push_front(TmToString(key));
        break;
      }
    }
    return idx->second;
  }
  return std::nullopt;
}

}  // namespace wfa_virtual_people