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

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "google/type/date.pb.h"
#include "wfa/measurement/api/v2alpha/model_line.pb.h"
#include "wfa/measurement/api/v2alpha/model_rollout.pb.h"
#include "wfa/virtual_people/common/event.pb.h"
#include "wfa/virtual_people/core/selector/lru_cache.h"

namespace wfa_virtual_people {

using ::wfa::measurement::api::v2alpha::ModelLine;
using ::wfa::measurement::api::v2alpha::ModelRollout;

class VidModelSelector {
 public:
  // Factory method to create an instance of `VidModelSelector`.
  //
  // Returns an error if model_line name is unspecified or invalid and if
  // model_rollout is parented by a different model_line.
  static absl::StatusOr<std::unique_ptr<VidModelSelector>> Build(
      const ModelLine& model_line,
      const std::vector<ModelRollout>& model_rollouts);

  absl::StatusOr<std::unique_ptr<std::string>> GetModelRelease(
      const LabelerInput& labeler_input);

  // Class constructor. Never call the constructor directly.
  // Instances of this class must be built using the factory method `Build`.
  VidModelSelector(const ModelLine& model_line,
                   const std::vector<ModelRollout>& model_rollouts);

 private:
  ModelLine model_line;
  std::vector<ModelRollout> model_rollouts;
  LruCache lru_cache;
  std::mutex mtx;

  // Access to the cache is synchronized to prevent multiple threads calculating
  // percentages in case of cache miss.
  std::vector<ModelReleasePercentile> ReadFromCache(std::tm& event_date_utc);

  // Return a list of ModelReleasePercentile(s). Each ModelReleasePercentile
  // wraps the percentage of adoption of a particular ModelRelease and the
  // ModelRelease itself. The list is sorted by either rollout_period_start_date
  // or instant_rollout_date.
  //
  // The adoption percentage of each ModelRollout is calculated as follows:
  // (EVENT_DAY - ROLLOUT_START_DAY) / (ROLLOUT_END_DAY - ROLLOUT_START_DAY).
  //
  // In case a ModelRollout has the `rollout_freeze_date` set and the event day
  // is greater than rollout_freeze_date, the EVENT_DAY in the above formula is
  // replaced by `rollout_freeze_date` to ensure that the rollout stops its
  // expansion: (ROLLOUT_FREEZE_DATE - ROLLOUT_START_DAY) / (ROLLOUT_END_DAY -
  // ROLLOUT_START_DAY).
  //
  // In case of an instant rollout ROLLOUT_START_DATE is equal to
  // ROLLOUT_END_DATE.
  std::vector<ModelReleasePercentile> CalculatePercentages(
      std::tm& event_date_utc);

  // Returns the percentage of events that this ModelRollout must label for the
  // given `event_date_utc`.
  double CalculatePercentageAdoption(std::tm& event_date_utc,
                                     const ModelRollout& model_rollout);

  // Iterates through all available ModelRollout(s) sorted by either
  // `rollout_period_start_date` or `instant_rollout_date` from the most recent
  // to the oldest. The function keeps adding ModelRollout(s) to the
  // `active_rollouts` vector until the following condition is met:
  // event_date_utc
  // >= rollout_period_end_date && !rollout.has_rollout_freeze_date()
  std::vector<ModelRollout> RetrieveActiveRollouts(std::tm& event_date_utc);
  absl::StatusOr<std::unique_ptr<std::string>> GetEventId(
      const LabelerInput& labeler_input);

  // Converts a given TimestampUsec into a std::tm object
  // std::tm is used to compare date in UTC time.
  std::tm TimestampUsecToTm(std::int64_t timestamp_usec);

  // Converts a google::type::Date object into a std:tm object.
  std::tm DateToTm(const google::type::Date& date);

  // Returns true if the given dates represent the same day.
  // It performs a lexicographically check, which is more efficient than
  // converting both date into std::time_t
  bool IsSameDate(const std::tm& date1, const std::tm& date2) const;

  // Returns true if date1 is older in time compare to date2.
  // It performs a lexicographically check, which is more efficient than
  // converting both date into std::time_t
  bool IsOlderDate(const std::tm& date1, const std::tm& date2) const;

  // Comparator used to sort a std::vector of `ModelRollout`.
  //
  // If `ModelRollout` has gradual rollout period use the `start_date`.
  // Otherwise use the `instant_rollout_date`.
  bool CompareModelRollouts(const ModelRollout& lhs, const ModelRollout& rhs);

  // Returns the model_line_id from the given resource name.
  // Returns an empty string if not model_line_id is found.
  static std::string ReadModelLine(const std::string& input);
};

}  // namespace wfa_virtual_people

#endif  // SRC_MAIN_CC_WFA_VIRTUAL_PEOPLE_CORE_SELECTOR_VID_MODEL_SELECTOR_H_
