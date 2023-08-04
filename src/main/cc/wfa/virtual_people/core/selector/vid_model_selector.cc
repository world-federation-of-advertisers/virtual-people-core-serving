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

#include "wfa/virtual_people/core/selector/vid_model_selector.h"

#include <stdexcept>

namespace wfa_virtual_people {

const int CACHE_SIZE = 60;

VidModelSelector::VidModelSelector(const ModelLine& model_line, const std::list<ModelRollout>& model_rollouts) : model_line(model_line), model_rollouts(model_rollouts), lru_cache(CACHE_SIZE) {}

std::list<ModelReleasePercentile> VidModelSelector::readFromCache(const std::tm& eventDateUtc) {
  std::lock_guard<std::mutex> lock(mtx);

  std::optional<std::list<ModelReleasePercentile>> cache_value = lru_cache.Get(eventDateUtc);
  if (cache_value.has_value()) {
    return cache_value.value();
  } else {
    std::list<ModelReleasePercentile> percentages = calculatePercentages(eventDateUtc);
    lru_cache.Add(eventDateUtc, percentages);
    return percentages;
  }
}

std::list<ModelReleasePercentile> VidModelSelector::calculatePercentages(const std::tm& eventDateUtc) {
  std::list<ModelRollout> activeRollouts = retrieveActiveRollouts(eventDateUtc);
  std::list<ModelReleasePercentile> result;

  for (const ModelRollout& activeRollout : activeRollouts) {
      double percentage = calculatePercentageAdoption(eventDateUtc, activeRollout);
      ModelReleasePercentile releasePercentile{percentage, activeRollout.model_release()};
      result.push_back(releasePercentile);
  }

  return result;
}

std::string GetEventId(LabelerInput& labeler_input) {

  if (labeler_input.has_profile_info()) {
    const ProfileInfo* profile_info = &labeler_input.profile_info();

    if (profile_info->has_email_user_info() && profile_info->email_user_info().has_user_id()) {
      return profile_info->email_user_info().user_id();
    }
    if (profile_info->has_phone_user_info() && profile_info->phone_user_info().has_user_id()) {
      return profile_info->phone_user_info().user_id();
    }
    if (profile_info->has_logged_in_id_user_info() && profile_info->logged_in_id_user_info().has_user_id()) {
      return profile_info->logged_in_id_user_info().user_id();
    }
    if (profile_info->has_logged_out_id_user_info() && profile_info->logged_out_id_user_info().has_user_id()) {
      return profile_info->logged_out_id_user_info().user_id();
    }
    if (profile_info->has_proprietary_id_space_1_user_info() && profile_info->proprietary_id_space_1_user_info().has_user_id()) {
      return profile_info->proprietary_id_space_1_user_info().user_id();
    }
    if (profile_info->has_proprietary_id_space_2_user_info() && profile_info->proprietary_id_space_2_user_info().has_user_id()) {
      return profile_info->proprietary_id_space_2_user_info().user_id();
    }
    if (profile_info->has_proprietary_id_space_3_user_info() && profile_info->proprietary_id_space_3_user_info().has_user_id()) {
      return profile_info->proprietary_id_space_3_user_info().user_id();
    }
    if (profile_info->has_proprietary_id_space_4_user_info() && profile_info->proprietary_id_space_4_user_info().has_user_id()) {
      return profile_info->proprietary_id_space_4_user_info().user_id();
    }
    if (profile_info->has_proprietary_id_space_5_user_info() && profile_info->proprietary_id_space_5_user_info().has_user_id()) {
      return profile_info->proprietary_id_space_5_user_info().user_id();
    }
    if (profile_info->has_proprietary_id_space_6_user_info() && profile_info->proprietary_id_space_6_user_info().has_user_id()) {
      return profile_info->proprietary_id_space_6_user_info().user_id();
    }
    if (profile_info->has_proprietary_id_space_7_user_info() && profile_info->proprietary_id_space_7_user_info().has_user_id()) {
      return profile_info->proprietary_id_space_7_user_info().user_id();
    }
    if (profile_info->has_proprietary_id_space_8_user_info() && profile_info->proprietary_id_space_8_user_info().has_user_id()) {
      return profile_info->proprietary_id_space_8_user_info().user_id();
    }
    if (profile_info->has_proprietary_id_space_9_user_info() && profile_info->proprietary_id_space_9_user_info().has_user_id()) {
      return profile_info->proprietary_id_space_9_user_info().user_id();
    }
    if (profile_info->has_proprietary_id_space_10_user_info() && profile_info->proprietary_id_space_10_user_info().has_user_id()) {
      return profile_info->proprietary_id_space_10_user_info().user_id();
    }
  } else if(labeler_input.has_event_id()) {
    return labeler_input.event_id().id();
  }
  throw std::runtime_error("Neither user_id nor event_id was found in the LabelerInput.");
}

} // namespace wfa_virtual_people