// Copyright 2021 The Cross-Media Measurement Authors
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

#include "wfa/virtual_people/core/model/utils/distributed_consistent_hashing.h"

#include <cmath>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "src/farmhash.h"

namespace wfa_virtual_people {

absl::StatusOr<std::unique_ptr<DistributedConsistentHashing>>
DistributedConsistentHashing::Build(
    std::unique_ptr<std::vector<DistributionChoice>> distribution) {
  if (distribution->empty()) {
    return absl::InvalidArgumentError("The given distribution is empty.");
  }
  // Returns error status for any negative probability.
  auto negative_probability_it = std::find_if(
      distribution->begin(), distribution->end(),
      [](const DistributionChoice& choice) {
        return (choice.probability < 0);
      });
  if (negative_probability_it != distribution->end()) {
    return absl::InvalidArgumentError("Negative probability is provided.");
  }

  // Gets sum of probabilities.
  double probabilities_sum = 0.0;
  for_each (distribution->begin(), distribution->end(),
            [&probabilities_sum](const DistributionChoice& choice) {
    probabilities_sum += choice.probability;
  });
  if (probabilities_sum <= 0) {
    return absl::InvalidArgumentError("Probabilities sum is not positive.");
  }
  // Normalizes the probabilities.
  for_each (distribution->begin(), distribution->end(),
            [probabilities_sum](DistributionChoice& choice) {
    choice.probability /= probabilities_sum;
  });
  return absl::make_unique<DistributedConsistentHashing>(
      std::move(distribution));
}

std::string GetFullSeed(
    absl::string_view random_seed, const int32_t choice) {
  return absl::StrFormat("consistent-hashing-%s-%d", random_seed, choice);
}

inline double FloatHash(absl::string_view full_seed) {
  return static_cast<double>(util::Fingerprint64(full_seed)) /
         static_cast<double>(std::numeric_limits<uint64_t>::max());
}

double ComputeXi(
    absl::string_view random_seed,
    const int32_t choice, const double probability) {
  return (-std::log(FloatHash(GetFullSeed(random_seed, choice))) /
          probability);
}

// A C++ version of the Python function ConsistentHashing.hash in
// https://github.com/world-federation-of-advertisers/virtual_people_examples/blob/main/notebooks/Consistent_Hashing.ipynb
int32_t DistributedConsistentHashing::Hash(
    absl::string_view random_seed) const {
  auto it = distribution_->begin();
  int32_t choice = it->choice_id;
  double choice_xi = ComputeXi(random_seed, it->choice_id, it->probability);
  for (++it; it != distribution_->end(); ++it) {
    double xi = ComputeXi(random_seed, it->choice_id, it->probability);
    if (choice_xi > xi) {
      choice = it->choice_id;
      choice_xi = xi;
    }
  }
  return choice;
}

}  // namespace wfa_virtual_people
