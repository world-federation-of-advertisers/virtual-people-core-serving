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

#include <iostream>
#include <regex>
#include <cmath>
#include <google/protobuf/util/time_util.h>

namespace wfa_virtual_people {

const int CACHE_SIZE = 60;

const double UPPER_BOUND_PERCENTAGE_ADOPTION = 1.1;

std::string VidModelSelector::ExtractModelLine(const std::string& input) {
    std::regex pattern(R"(\bmodelLines/([^/]+)\b)");
    std::smatch match;
    if (std::regex_search(input, match, pattern)) {
        return match[1];
    } else {
        return "";
    }
}

std::tm VidModelSelector::TimestampUsecToTm(std::int64_t timestamp_usec) {
  const std::time_t timestamp_sec = static_cast<std::time_t>(timestamp_usec / 1000000);
  const std::tm* utc_tm = std::gmtime(&timestamp_sec);
  std::tm result = {};
  result.tm_year = utc_tm->tm_year + 1900;
  result.tm_mon = utc_tm->tm_mon + 1;
  result.tm_mday = utc_tm->tm_mday;
  return result;
}

std::tm VidModelSelector::DateToTm(const google::type::Date& date) {
    std::tm tmDate = {};
    tmDate.tm_year = date.year();
    tmDate.tm_mon = date.month();
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

VidModelSelector::VidModelSelector(const ModelLine& model_line, const std::vector<ModelRollout>& model_rollouts)
    : lru_cache(CACHE_SIZE) {

    std::string model_line_id = ExtractModelLine(model_line.name());
    if (model_line_id == "") {
        throw std::invalid_argument("ModelLine resource name is either unspecified or invalid");
    }
    for (auto model_rollout = model_rollouts.begin(); model_rollout != model_rollouts.end(); ++model_rollout) {
        if(ExtractModelLine(model_rollout->name()) != model_line_id){
            throw std::invalid_argument("ModelRollouts must be parented by the provided ModelLine");
        }
    }

    this->model_line = model_line;
    this->model_rollouts = model_rollouts;
}

std::optional<std::string> VidModelSelector::GetModelRelease(const LabelerInput* labeler_input) {
    std::int64_t event_timestamp_usec = labeler_input->timestamp_usec();
    std::int64_t model_line_active_start_time = google::protobuf::util::TimeUtil::TimestampToMicroseconds(model_line.active_start_time());
    std::int64_t model_line_active_end_time = model_line.has_active_end_time() ?
        google::protobuf::util::TimeUtil::TimestampToMicroseconds(model_line.active_end_time()) :
        ((1LL << 63) - 1);

    if (event_timestamp_usec >= model_line_active_start_time && event_timestamp_usec < model_line_active_end_time) {
        std::tm event_date_utc = TimestampUsecToTm(event_timestamp_usec);
        std::vector<ModelReleasePercentile> model_adoption_percentages = ReadFromCache(event_date_utc);
        std::string selected_model_release;
        if (model_adoption_percentages.empty()) {
            return std::nullopt;
        } else {
            selected_model_release = model_adoption_percentages[0].model_release_resource_key;
        }
        std::string event_id = GetEventId(*labeler_input);
        for (auto percentage = model_adoption_percentages.begin(); percentage != model_adoption_percentages.end(); ++percentage) {
            std::string string_to_hash;
            string_to_hash += percentage->model_release_resource_key;
            string_to_hash += event_id;
            int64_t event_fingerprint = static_cast<int64_t>(util::Fingerprint64(string_to_hash));

            double reduced_event_id = std::abs(static_cast<double>(event_fingerprint) / std::numeric_limits<long long>::max());
            if (reduced_event_id < percentage->end_percentile) {
                selected_model_release = percentage->model_release_resource_key;
            }
        }
        return selected_model_release;
    } else {
        return std::nullopt;
    }
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
        DateToTm(model_rollout.rollout_freeze_date()) :
        [] {
            std::tm max_date_tm = {};
            max_date_tm.tm_year = 9999 - 1900;
            max_date_tm.tm_mon = 11;
            max_date_tm.tm_mday = 31;
            return max_date_tm;
            }();

    std::tm rollout_period_start_date = model_rollout.has_gradual_rollout_period() ?
        DateToTm(model_rollout.gradual_rollout_period().start_date()) : DateToTm(model_rollout.instant_rollout_date());

    std::tm rollout_period_end_date = model_rollout.has_gradual_rollout_period() ?
        DateToTm(model_rollout.gradual_rollout_period().end_date()) : DateToTm(model_rollout.instant_rollout_date());

    if (IsSameDate(rollout_period_start_date, rollout_period_end_date)) {
        return UPPER_BOUND_PERCENTAGE_ADOPTION;
    } else if (!IsOlderDate(event_date_utc, model_rollout_freeze_date)) {
        return (difftime(std::mktime(&model_rollout_freeze_date), std::mktime(&rollout_period_start_date))) /
            (difftime(std::mktime(&rollout_period_end_date), std::mktime(&rollout_period_start_date)));
    } else {
        return (difftime(std::mktime(&event_date_utc), std::mktime(&rollout_period_start_date))) /
            (difftime(std::mktime(&rollout_period_end_date), std::mktime(&rollout_period_start_date)));
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
    std::tm first_rollout_period_start_date = first_rollout.has_gradual_rollout_period() ?
        DateToTm(first_rollout.gradual_rollout_period().start_date()) :
        DateToTm(first_rollout.instant_rollout_date());

    if (IsOlderDate(event_date_utc, first_rollout_period_start_date)) {
        return {};
    }

    std::vector<ModelRollout> active_rollouts;

    for (auto rollout_idx = sorted_rollouts.rbegin(); rollout_idx != sorted_rollouts.rend(); ++rollout_idx) {
        const ModelRollout& rollout = *rollout_idx;

        std::tm rollout_period_end_date = rollout.has_gradual_rollout_period() ?
            DateToTm(rollout.gradual_rollout_period().end_date()) :
            DateToTm(rollout.instant_rollout_date());

        if (!IsOlderDate(event_date_utc, rollout_period_end_date)) {
            active_rollouts.push_back(rollout);
            if (!rollout.has_rollout_freeze_date()) {
                break;
            }
            continue;
        }

        std::tm rollout_period_start_date = rollout.has_gradual_rollout_period() ?
            DateToTm(rollout.gradual_rollout_period().start_date()) :
            DateToTm(rollout.instant_rollout_date());

        if (!IsOlderDate(event_date_utc, rollout_period_start_date)) {
            active_rollouts.push_back(rollout);
        }
    }

    std::reverse(active_rollouts.begin(), active_rollouts.end());
    return active_rollouts;

}

std::string VidModelSelector::GetEventId(const LabelerInput& labeler_input) {

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