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

#ifndef SRC_MAIN_CC_WFA_VIRTUAL_PEOPLE_CORE_SELECTOR_VID_MODEL_SELECTOR_H_
#define SRC_MAIN_CC_WFA_VIRTUAL_PEOPLE_CORE_SELECTOR_VID_MODEL_SELECTOR_H_

#include <list>
#include <string>

#include "wfa/measurement/api/v2alpha/model_line.pb.h"
#include "wfa/measurement/api/v2alpha/model_rollout.pb.h"
#include "wfa/virtual_people/common/event.pb.h"
#include "wfa/virtual_people/core/selector/lru_cache.h"

namespace wfa_virtual_people {

using ::wfa::measurement::api::v2alpha::ModelLine;
using ::wfa::measurement::api::v2alpha::ModelRollout;

class VidModelSelector {
 public:
  VidModelSelector(const ModelLine& model_line, const std::list<ModelRollout>& model_rollouts);
  std::string getModelRelease(const LabelerInput* labelerInput);

 private:
  ModelLine model_line;
  std::list<ModelRollout> model_rollouts;
  LruCache lru_cache;

  std::list<ModelReleasePercentile> readFromCache(const std::tm& key);
  std::list<ModelReleasePercentile> calculatePercentages(const std::tm& key);
  double calculatePercentageAdoption(const std::tm& key, const &ModelRollout model_rollout);
  std::list<ModelRollout> retrieveActiveRollouts(const std::tm& key);
  std::string GetEventId(LabelerInput& labeler_input) ;
};

}  // namespace wfa_virtual_people

#endif  // SRC_MAIN_CC_WFA_VIRTUAL_PEOPLE_CORE_SELECTOR_VID_MODEL_SELECTOR_H_
