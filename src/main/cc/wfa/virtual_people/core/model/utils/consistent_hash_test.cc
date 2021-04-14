#include "wfa/virtual_people/core/model/utils/consistent_hash.h"

#include <random>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace wfa_virtual_people {
namespace core {
namespace utils {
namespace {

using ::testing::AnyOf;

constexpr int kKeyNumber = 1000;
constexpr int32_t kMaxBuckets = 1 << 16;

// When using the same key, for any number of buckets n > 1, one of the
// following must be satisfied
// * JumpConsistentHash(key, n) == JumpConsistentHash(key, n - 1)
// * JumpConsistentHash(key, n) == n - 1
void CheckCorrectnessForOneKey(const uint64_t key, const int32_t max_buckets) {
  int32_t last_output = 0;
  for (int32_t num_buckets = 1; num_buckets <= max_buckets; ++num_buckets) {
    int32_t output = JumpConsistentHash(key, num_buckets);
    EXPECT_THAT(output, AnyOf(last_output, num_buckets - 1))
        << "with key " << key << " and max_buckets " << max_buckets;
    last_output = output;
  }
}

TEST(JumpConsistentHashTest, TestCorrectness) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<unsigned long long> distrib(
      std::numeric_limits<std::uint64_t>::min(),
      std::numeric_limits<std::uint64_t>::max()
  );
  for (int i = 0; i < kKeyNumber; i++) {
    uint64_t key = static_cast<uint64_t>(distrib(gen));
    CheckCorrectnessForOneKey(key, kMaxBuckets);
  }
}

}  // namespace
}  // namespace utils
}  // namespace core
}  // namespace wfa_virtual_people
