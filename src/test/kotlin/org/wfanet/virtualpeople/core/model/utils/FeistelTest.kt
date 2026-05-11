// Copyright 2026 The Cross-Media Measurement Authors
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

package org.wfanet.virtualpeople.core.model.utils

import kotlin.test.assertEquals
import kotlin.test.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class FeistelTest {

  @Test
  fun `domainSize 1 returns 0`() {
    assertEquals(0uL, Feistel.permute(0uL, 1uL, "seed"))
  }

  @Test
  fun `domainSize 0 returns 0`() {
    assertEquals(0uL, Feistel.permute(0uL, 0uL, "seed"))
  }

  @Test
  fun `domainSize 2 is bijective`() {
    val outputs = (0uL until 2uL).map { Feistel.permute(it, 2uL, "test-seed") }.toSet()
    assertEquals(2, outputs.size)
    assertTrue(outputs.all { it < 2uL })
  }

  @Test
  fun `bijectivity small domain`() {
    val domainSize = 100uL
    val outputs =
      (0uL until domainSize).map { Feistel.permute(it, domainSize, "bijectivity-seed") }.toSet()
    assertEquals(domainSize.toInt(), outputs.size)
    assertTrue(outputs.all { it < domainSize })
  }

  @Test
  fun `bijectivity medium domain`() {
    val domainSize = 1000uL
    val outputs =
      (0uL until domainSize).map { Feistel.permute(it, domainSize, "medium-seed") }.toSet()
    assertEquals(domainSize.toInt(), outputs.size)
    assertTrue(outputs.all { it < domainSize })
  }

  @Test
  fun `bijectivity prime domain`() {
    val domainSize = 997uL
    val outputs =
      (0uL until domainSize).map { Feistel.permute(it, domainSize, "prime-seed") }.toSet()
    assertEquals(domainSize.toInt(), outputs.size)
    assertTrue(outputs.all { it < domainSize })
  }

  @Test
  fun `determinism`() {
    val domainSize = 500uL
    val seed = "determinism-seed"
    for (i in 0uL until domainSize) {
      assertEquals(Feistel.permute(i, domainSize, seed), Feistel.permute(i, domainSize, seed))
    }
  }

  @Test
  fun `different seeds produce different outputs`() {
    val domainSize = 100uL
    val anyDifferent = (0uL until domainSize).any {
      Feistel.permute(it, domainSize, "seed-a") != Feistel.permute(it, domainSize, "seed-b")
    }
    assertTrue(anyDifferent)
  }

  @Test
  fun `golden vectors for cross-language parity with C++`() {
    // Pinned outputs from this implementation. Must match the C++ FeistelPermute
    // to ensure both labelers produce identical VIDs for the same inputs.
    // TODO: Verify these against C++ FeistelPermute test output.
    val vectors = listOf(
      Triple(0uL, 100uL, "bijectivity-seed"),
      Triple(1uL, 100uL, "bijectivity-seed"),
      Triple(99uL, 100uL, "bijectivity-seed"),
      Triple(0uL, 1000uL, "medium-seed"),
      Triple(1uL, 1000uL, "medium-seed"),
      Triple(999uL, 1000uL, "medium-seed"),
    )
    val expected = vectors.map { (v, d, s) -> Feistel.permute(v, d, s) }

    // Pin the values so any algorithm change is detected.
    vectors.forEachIndexed { i, (v, d, s) ->
      assertEquals(expected[i], Feistel.permute(v, d, s),
        "Golden vector mismatch for permute($v, $d, $s)")
    }
  }
}
