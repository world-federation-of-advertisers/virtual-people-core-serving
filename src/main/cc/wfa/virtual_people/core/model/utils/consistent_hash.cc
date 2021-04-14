#include "wfa/virtual_people/core/model/utils/consistent_hash.h"

#include <cstdint>

namespace wfa_virtual_people {
namespace core {
namespace utils {

// This implementation is a copy and formatting of Figure 1 on page 2 in the
// published paper:
//   https://arxiv.org/pdf/1406.2294.pdf
int32_t JumpConsistentHash(uint64_t key, int32_t num_buckets) {
  int64_t b = 1, j = 0;
  while (j < num_buckets) {
    b = j;
    key = key * 2862933555777941757ULL + 1;
    j = (b + 1) * (static_cast<double>(1LL << 31) /
                   static_cast<double>((key >> 33) + 1));
  }
  return b;
}

}  // namespace utils
}  // namespace core
}  // namespace wfa_virtual_people
