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

#include <google/protobuf/util/time_util.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "common_cpp/macros/macros.h"
#include "src/farmhash.h"

namespace wfa_virtual_people {

const int kCacheSize = 60;

const double UPPER_BOUND_PERCENTAGE_ADOPTION = 1.1;

std::string VidModelSelector::ReadModelLine(const std::string& input) {
  std::string modelLineMarker = "modelLines/";
  std::string modelRolloutMarker = "/modelRollouts/";

  size_t start = input.find(modelLineMarker);
  if (start != std::string::npos) {
    start += modelLineMarker.length();
    size_t end = input.find(modelRolloutMarker, start);
    if (end != std::string::npos) {
      return input.substr(start, end - start);
    } else {
      return input.substr(start);
    }
  }

  return "";
}

absl::CivilDay VidModelSelector::TimestampUsecToCivilDay(std::int64_t timestamp_usec) {
  absl::Time timestamp = absl::FromUnixMicros(timestamp_usec);
  absl::TimeZone utc_time_zone = absl::UTCTimeZone();
  return absl::ToCivilDay(timestamp, utc_time_zone);
}

absl::CivilDay VidModelSelector::DateToCivilDay(const google::type::Date& date) {
  return absl::CivilDay(date.year(), date.month(), date.day());
}

double VidModelSelector::GetTimeDifferenceInSeconds(absl::CivilDay& date_1, absl::CivilDay& date_2) {
  absl::Time time_1 = absl::FromCivil(date_1, absl::UTCTimeZone());
  absl::Time time_2 = absl::FromCivil(date_2, absl::UTCTimeZone());
  absl::Duration difference = time_2 - time_1;
  return absl::ToDoubleSeconds(difference);
}

absl::StatusOr<std::unique_ptr<VidModelSelector>> VidModelSelector::Build(
    const ModelLine& model_line,
    const std::vector<ModelRollout>& model_rollouts) {
  std::string model_line_id = ReadModelLine(model_line.name());
  if (model_line_id == "") {
    return absl::InvalidArgumentError(
        "ModelLine resource name is either unspecified or invalid");
  }
  for (const ModelRollout& model_rollout : model_rollouts) {
    if (ReadModelLine(model_rollout.name()) != model_line_id) {
      return absl::InvalidArgumentError(
          "ModelRollouts must be parented by the provided ModelLine");
    }
  }

  auto selector =
      absl::make_unique<VidModelSelector>(model_line, model_rollouts);
  return std::move(selector);
}

VidModelSelector::VidModelSelector(
    const ModelLine& model_line,
    const std::vector<ModelRollout>& model_rollouts)
    : model_line(model_line),
      model_rollouts(model_rollouts),
      lru_cache(kCacheSize) {}

absl::StatusOr<std::unique_ptr<std::string>> VidModelSelector::GetModelRelease(
    const LabelerInput& labeler_input) {
  std::int64_t event_timestamp_usec = labeler_input.timestamp_usec();
  std::int64_t model_line_active_start_time =
      google::protobuf::util::TimeUtil::TimestampToMicroseconds(
          model_line.active_start_time());
  std::int64_t model_line_active_end_time =
      model_line.has_active_end_time()
          ? google::protobuf::util::TimeUtil::TimestampToMicroseconds(
                model_line.active_end_time())
          : ((1LL << 63) - 1);

  if (event_timestamp_usec >= model_line_active_start_time &&
      event_timestamp_usec < model_line_active_end_time) {
    absl::CivilDay event_date_utc = TimestampUsecToCivilDay(event_timestamp_usec);
    std::vector<ModelReleasePercentile> model_adoption_percentages =
        ReadFromCache(event_date_utc);
    std::string selected_model_release;
    if (model_adoption_percentages.empty()) {
      return std::unique_ptr<std::string>(nullptr);
    } else {
      selected_model_release =
          model_adoption_percentages[0].model_release_resource_key;
    }
    std::unique_ptr<std::string> event_id = nullptr;
    ASSIGN_OR_RETURN(event_id, GetEventId(labeler_input));

    for (auto percentage = model_adoption_percentages.begin();
         percentage != model_adoption_percentages.end(); ++percentage) {
      std::string string_to_hash;
      string_to_hash += percentage->model_release_resource_key;
      string_to_hash += *event_id;
      // Convert into a signed number to make sure that the event_fingerprint is
      // the same one produced by the Kotlin library.
      int64_t event_fingerprint =
          static_cast<int64_t>(util::Fingerprint64(string_to_hash));

      double reduced_event_id =
          std::abs(static_cast<double>(event_fingerprint) /
                   std::numeric_limits<int64_t>::max());
      if (reduced_event_id < percentage->end_percentile) {
        selected_model_release = percentage->model_release_resource_key;
      }
    }
    return std::make_unique<std::string>(std::move(selected_model_release));
  } else {
    return std::unique_ptr<std::string>(nullptr);
  }
}

std::vector<ModelReleasePercentile> VidModelSelector::ReadFromCache(
    absl::CivilDay& event_date_utc) {
  std::lock_guard<std::mutex> lock(mtx);

  std::optional<std::vector<ModelReleasePercentile>> cache_value =
      lru_cache.Get(event_date_utc);
  if (cache_value.has_value()) {
    return cache_value.value();
  } else {
    std::vector<ModelReleasePercentile> percentages =
        CalculatePercentages(event_date_utc);
    lru_cache.Add(event_date_utc, percentages);
    return percentages;
  }
}

std::vector<ModelReleasePercentile> VidModelSelector::CalculatePercentages(
    absl::CivilDay& event_date_utc) {
  std::vector<ModelRollout> active_rollouts =
      RetrieveActiveRollouts(event_date_utc);
  std::vector<ModelReleasePercentile> result;

  for (const ModelRollout& active_rollout : active_rollouts) {
    double percentage =
        CalculatePercentageAdoption(event_date_utc, active_rollout);
    ModelReleasePercentile release_percentile{percentage,
                                              active_rollout.model_release()};
    result.push_back(release_percentile);
  }

  return result;
}

double VidModelSelector::CalculatePercentageAdoption(
    absl::CivilDay& event_date_utc, const ModelRollout& model_rollout) {
  absl::CivilDay model_rollout_freeze_date =
      model_rollout.has_rollout_freeze_date()
          ? DateToCivilDay(model_rollout.rollout_freeze_date())
          : absl::CivilDay(absl::CivilSecond::max());
  absl::CivilDay rollout_period_start_date =
      model_rollout.has_gradual_rollout_period()
          ? DateToCivilDay(model_rollout.gradual_rollout_period().start_date())
          : DateToCivilDay(model_rollout.instant_rollout_date());

  absl::CivilDay rollout_period_end_date =
      model_rollout.has_gradual_rollout_period()
          ? DateToCivilDay(model_rollout.gradual_rollout_period().end_date())
          : DateToCivilDay(model_rollout.instant_rollout_date());

  if (rollout_period_start_date == rollout_period_end_date) {
    return UPPER_BOUND_PERCENTAGE_ADOPTION;
  } else if (event_date_utc >= model_rollout_freeze_date) {
    return (GetTimeDifferenceInSeconds(rollout_period_start_date, model_rollout_freeze_date)) / (GetTimeDifferenceInSeconds(rollout_period_start_date, rollout_period_end_date));
  } else {
    return (GetTimeDifferenceInSeconds(rollout_period_start_date, event_date_utc)) / (GetTimeDifferenceInSeconds(rollout_period_start_date, rollout_period_end_date));
  }
}

bool VidModelSelector::CompareModelRollouts(const ModelRollout& lhs,
                                            const ModelRollout& rhs) {
  const absl::CivilDay lhsDate =
      lhs.has_gradual_rollout_period()
          ? DateToCivilDay(lhs.gradual_rollout_period().start_date())
          : DateToCivilDay(lhs.instant_rollout_date());
  const absl::CivilDay rhsDate =
      rhs.has_gradual_rollout_period()
          ? DateToCivilDay(rhs.gradual_rollout_period().start_date())
          : DateToCivilDay(rhs.instant_rollout_date());
  return lhsDate < rhsDate;
}

std::vector<ModelRollout> VidModelSelector::RetrieveActiveRollouts(
    absl::CivilDay& event_date_utc) {
  if (model_rollouts.empty()) {
    return model_rollouts;
  }

  std::vector<ModelRollout> sorted_rollouts = model_rollouts;
  std::sort(sorted_rollouts.begin(), sorted_rollouts.end(),
            [this](const ModelRollout& lhs, const ModelRollout& rhs) {
              return CompareModelRollouts(lhs, rhs);
            });

  const ModelRollout& first_rollout = sorted_rollouts.front();
  absl::CivilDay first_rollout_period_start_date =
      first_rollout.has_gradual_rollout_period()
          ? DateToCivilDay(first_rollout.gradual_rollout_period().start_date())
          : DateToCivilDay(first_rollout.instant_rollout_date());

  if (event_date_utc < first_rollout_period_start_date) {
    return {};
  }

  std::vector<ModelRollout> active_rollouts;

  for (auto rollout_idx = sorted_rollouts.rbegin();
       rollout_idx != sorted_rollouts.rend(); ++rollout_idx) {
    const ModelRollout& rollout = *rollout_idx;

    absl::CivilDay rollout_period_end_date =
        rollout.has_gradual_rollout_period()
            ? DateToCivilDay(rollout.gradual_rollout_period().end_date())
            : DateToCivilDay(rollout.instant_rollout_date());

    if (event_date_utc >= rollout_period_end_date) {
      active_rollouts.push_back(rollout);
      if (!rollout.has_rollout_freeze_date()) {
        break;
      }
      continue;
    }

    absl::CivilDay rollout_period_start_date =
        rollout.has_gradual_rollout_period()
            ? DateToCivilDay(rollout.gradual_rollout_period().start_date())
            : DateToCivilDay(rollout.instant_rollout_date());

    if (event_date_utc >= rollout_period_start_date) {
      active_rollouts.push_back(rollout);
    }
  }

  std::reverse(active_rollouts.begin(), active_rollouts.end());
  return active_rollouts;
}

absl::StatusOr<std::unique_ptr<std::string>> VidModelSelector::GetEventId(
    const LabelerInput& labeler_input) {
  if (labeler_input.has_profile_info()) {
    const ProfileInfo* profile_info = &labeler_input.profile_info();

    if (profile_info->has_email_user_info() &&
        profile_info->email_user_info().has_user_id()) {
      return std::make_unique<std::string>(
          std::move(profile_info->email_user_info().user_id()));
    }
    if (profile_info->has_phone_user_info() &&
        profile_info->phone_user_info().has_user_id()) {
      return std::make_unique<std::string>(
          std::move(profile_info->phone_user_info().user_id()));
    }
    if (profile_info->has_logged_in_id_user_info() &&
        profile_info->logged_in_id_user_info().has_user_id()) {
      return std::make_unique<std::string>(
          std::move(profile_info->logged_in_id_user_info().user_id()));
    }
    if (profile_info->has_logged_out_id_user_info() &&
        profile_info->logged_out_id_user_info().has_user_id()) {
      return std::make_unique<std::string>(
          std::move(profile_info->logged_out_id_user_info().user_id()));
    }
    if (profile_info->has_proprietary_id_space_1_user_info() &&
        profile_info->proprietary_id_space_1_user_info().has_user_id()) {
      return std::make_unique<std::string>(std::move(
          profile_info->proprietary_id_space_1_user_info().user_id()));
    }
    if (profile_info->has_proprietary_id_space_2_user_info() &&
        profile_info->proprietary_id_space_2_user_info().has_user_id()) {
      return std::make_unique<std::string>(std::move(
          profile_info->proprietary_id_space_2_user_info().user_id()));
    }
    if (profile_info->has_proprietary_id_space_3_user_info() &&
        profile_info->proprietary_id_space_3_user_info().has_user_id()) {
      return std::make_unique<std::string>(std::move(
          profile_info->proprietary_id_space_3_user_info().user_id()));
    }
    if (profile_info->has_proprietary_id_space_4_user_info() &&
        profile_info->proprietary_id_space_4_user_info().has_user_id()) {
      return std::make_unique<std::string>(std::move(
          profile_info->proprietary_id_space_4_user_info().user_id()));
    }
    if (profile_info->has_proprietary_id_space_5_user_info() &&
        profile_info->proprietary_id_space_5_user_info().has_user_id()) {
      return std::make_unique<std::string>(std::move(
          profile_info->proprietary_id_space_5_user_info().user_id()));
    }
    if (profile_info->has_proprietary_id_space_6_user_info() &&
        profile_info->proprietary_id_space_6_user_info().has_user_id()) {
      return std::make_unique<std::string>(std::move(
          profile_info->proprietary_id_space_6_user_info().user_id()));
    }
    if (profile_info->has_proprietary_id_space_7_user_info() &&
        profile_info->proprietary_id_space_7_user_info().has_user_id()) {
      return std::make_unique<std::string>(std::move(
          profile_info->proprietary_id_space_7_user_info().user_id()));
    }
    if (profile_info->has_proprietary_id_space_8_user_info() &&
        profile_info->proprietary_id_space_8_user_info().has_user_id()) {
      return std::make_unique<std::string>(std::move(
          profile_info->proprietary_id_space_8_user_info().user_id()));
    }
    if (profile_info->has_proprietary_id_space_9_user_info() &&
        profile_info->proprietary_id_space_9_user_info().has_user_id()) {
      return std::make_unique<std::string>(std::move(
          profile_info->proprietary_id_space_9_user_info().user_id()));
    }
    if (profile_info->has_proprietary_id_space_10_user_info() &&
        profile_info->proprietary_id_space_10_user_info().has_user_id()) {
      return std::make_unique<std::string>(std::move(
          profile_info->proprietary_id_space_10_user_info().user_id()));
    }
  } else if (labeler_input.has_event_id()) {
    return std::make_unique<std::string>(
        std::move(labeler_input.event_id().id()));
  }
  return absl::InvalidArgumentError(
      "Neither user_id nor event_id was found in the LabelerInput.");
}

}  // namespace wfa_virtual_people
