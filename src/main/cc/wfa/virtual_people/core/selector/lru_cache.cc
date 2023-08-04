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

void LruCache::Add(const std::tm& key, const std::list<ModelReleasePercentile>& data){
  if (cache_data.size() >= cache_size) {
    auto oldest = access_order.begin();
    for (auto idx = access_order.begin(); idx != access_order.end(); ++idx) {
      if (TmComparator()(*idx, *oldest)) {
        oldest = idx;
      }
    }
    cache_data.erase(*oldest);
    access_order.erase(oldest);
  }

  cache_data[key] = data;
  access_order.emplace_front(key);

}

std::optional<std::list<ModelReleasePercentile>> LruCache::Get(const std::tm& key){
  auto idx = cache_data.find(key);
  if (idx != cache_data.end()) {
    for (auto it = access_order.begin(); it != access_order.end(); ++it) {
        if (TmComparator()(*it, key) || TmComparator()(key, *it)) {
            access_order.erase(it);
            access_order.push_front(key);
            break;
        }
    }
    return idx->second;
  }
  return std::nullopt;
}

} // namespace wfa_virtual_people