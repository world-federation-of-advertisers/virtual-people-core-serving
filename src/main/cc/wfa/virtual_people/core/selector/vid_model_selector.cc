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

const std::tm MAX_DATE = {
    9999 - 1900,
    11,
    31
};

const double UPPER_BOUND_PERCENTAGE_ADOPTION = 1.1;

std::tm VidModelSelector::TimestampUsecToTm(std::int64_t timestamp_usec) {
  const std::time_t timestamp_sec = static_cast<std::time_t>(timestamp_usec / 1000000);
  const std::tm* utc_tm = std::gmtime(&timestamp_sec);
  std::tm result = {};
  result.tm_year = utc_tm->tm_year;
  result.tm_mon = utc_tm->tm_mon;
  result.tm_mday = utc_tm->tm_mday;
  return result;
}

std::tm VidModelSelector::DateToTm(const google::type::Date& date) {
    std::tm tmDate = {};
    tmDate.tm_year = date.year() - 1900;
    tmDate.tm_mon = date.month() - 1;
    tmDate.tm_mday = date.day();
    return tmDate;
}

bool VidModelSelector::IsSameDate(const std::tm& date1, const std::tm& date2) const  {
    return std::tie(date1.tm_year, date1.tm_mon, date1.tm_mday) ==
           std::tie(date2.tm_year, date2.tm_mon, date2.tm_mday);
}

bool VidModelSelector::IsOlderDate(const std::tm& date1, const std::tm& date2) const {
    return std::tie(date1.tm_year, date1.tm_mon, date1.tm_mday) <
           std::tie(date2.tm_year, date2.tm_mon, date2.tm_mday);
}

VidModelSelector::VidModelSelector(const ModelLine& model_line, const std::vector<ModelRollout>& model_rollouts) : model_line(model_line), model_rollouts(model_rollouts), lru_cache(CACHE_SIZE) {}

std::string VidModelSelector::GetModelRelease(const LabelerInput* labelerInput) {

}

std::vector<ModelReleasePercentile> VidModelSelector::ReadFromCache(std::tm& event_date_utc) {
  std::lock_guard<std::mutex> lock(mtx);

  std::optional<std::vector<ModelReleasePercentile>> cache_value = lru_cache.Get(event_date_utc);
  if (cache_value.has_value()) {
    return cache_value.value();
  } else {
    std::vector<ModelReleasePercentile> percentages = CalculatePercentages(event_date_utc);
    lru_cache.Add(event_date_utc, percentages);
    return percentages;
  }
}

std::vector<ModelReleasePercentile> VidModelSelector::CalculatePercentages(std::tm& event_date_utc) {
  std::vector<ModelRollout> active_rollouts = RetrieveActiveRollouts(event_date_utc);
  std::vector<ModelReleasePercentile> result;

  for (const ModelRollout& active_rollout : active_rollouts) {
      double percentage = CalculatePercentageAdoption(event_date_utc, active_rollout);
      ModelReleasePercentile release_percentile{percentage, active_rollout.model_release()};
      result.push_back(release_percentile);
  }

  return result;
}

double VidModelSelector::CalculatePercentageAdoption(
    std::tm& event_date_utc,
    const ModelRollout& model_rollout
) {

    std::tm model_rollout_freeze_date = model_rollout.has_rollout_freeze_date() ?
        DateToTm(model_rollout.rollout_freeze_date()) : MAX_DATE;

    std::tm rollout_period_start_date = model_rollout.has_gradual_rollout_period() ?
        DateToTm(model_rollout.gradual_rollout_period().start_date()) : DateToTm(model_rollout.instant_rollout_date());

    std::tm rollout_period_end_date = model_rollout.has_gradual_rollout_period() ?
        DateToTm(model_rollout.gradual_rollout_period().end_date()) : DateToTm(model_rollout.instant_rollout_date());

    if (IsSameDate(rollout_period_start_date, rollout_period_end_date)) {
        return UPPER_BOUND_PERCENTAGE_ADOPTION;
    } else if (!IsOlderDate(event_date_utc, model_rollout_freeze_date)) {
        return ((difftime(std::mktime(&model_rollout_freeze_date), std::mktime(&rollout_period_start_date))) /
            (difftime(std::mktime(&rollout_period_end_date), std::mktime(&rollout_period_start_date))) / 86400);
    } else {
        return ((difftime(std::mktime(&event_date_utc), std::mktime(&rollout_period_start_date))) /
            (difftime(std::mktime(&rollout_period_end_date), std::mktime(&rollout_period_start_date))) / 86400);
    }
}


bool VidModelSelector::CompareModelRollouts(const ModelRollout& lhs, const ModelRollout& rhs) {
    const std::tm lhsDate = lhs.has_gradual_rollout_period() ? DateToTm(lhs.gradual_rollout_period().start_date()) : DateToTm(lhs.instant_rollout_date());
    const std::tm rhsDate = rhs.has_gradual_rollout_period() ? DateToTm(rhs.gradual_rollout_period().start_date()) : DateToTm(rhs.instant_rollout_date());
    return std::tie(lhsDate.tm_year, lhsDate.tm_mon, lhsDate.tm_mday) <
        std::tie(rhsDate.tm_year, rhsDate.tm_mon, rhsDate.tm_mday);
}

std::vector<ModelRollout> VidModelSelector::RetrieveActiveRollouts(std::tm& event_date_utc) {

    if (model_rollouts.empty()) {
        return model_rollouts;
    }

    std::vector<ModelRollout> sorted_rollouts = model_rollouts;
    std::sort(sorted_rollouts.begin(), sorted_rollouts.end(),  [this](const ModelRollout& lhs, const ModelRollout& rhs) {
        return CompareModelRollouts(lhs, rhs);
    });

    const ModelRollout& first_rollout = sorted_rollouts.front();
    std::tm first_rollout_period_start_date;
    if (first_rollout.has_gradual_rollout_period()) {
        first_rollout_period_start_date = DateToTm(first_rollout.gradual_rollout_period().start_date());
    } else {
        first_rollout_period_start_date = DateToTm(first_rollout.instant_rollout_date());
    }

    if (IsOlderDate(event_date_utc, first_rollout_period_start_date)) {
        return {};
    }

    std::vector<ModelRollout> active_rollouts;

    for (auto rollout_idx = sorted_rollouts.rbegin(); rollout_idx != sorted_rollouts.rend(); ++rollout_idx) {
        const ModelRollout& rollout = *rollout_idx;

        std::tm rollout_period_end_date;
        if (rollout.has_gradual_rollout_period()) {
            rollout_period_end_date = DateToTm(rollout.gradual_rollout_period().end_date());
        } else {
            rollout_period_end_date = DateToTm(rollout.instant_rollout_date());
        }

        if (!IsOlderDate(event_date_utc, rollout_period_end_date)) {
            active_rollouts.push_back(rollout);
            if (!rollout.has_rollout_freeze_date()) {
                break;
            }
            continue;
        }

        std::tm rollout_period_start_date;
        if (rollout.has_gradual_rollout_period()) {
            rollout_period_start_date = DateToTm(rollout.gradual_rollout_period().start_date());
        } else {
            rollout_period_start_date = DateToTm(rollout.instant_rollout_date());
        }

        if (!IsOlderDate(event_date_utc, rollout_period_start_date)) {
            active_rollouts.push_back(rollout);
        }
    }

    std::reverse(active_rollouts.begin(), active_rollouts.end());
    return active_rollouts;

}

std::string VidModelSelector::GetEventId(LabelerInput& labeler_input) {

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