// Copyright 2022 The Cross-Media Measurement Authors
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
// limitations under the \License.

package org.wfanet.virtualpeople.core.model.utils

import kotlin.test.assertFailsWith
import kotlin.test.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import org.wfanet.virtualpeople.common.labelerEvent

@RunWith(JUnit4::class)
class UpdateMatrixHelperTest {

  @Test
  fun `no matchers should throw`() {
    val exception =
      assertFailsWith<IllegalStateException> {
        selectFromMatrix(
          hashMatcher = null,
          filtersMatcher = null,
          rowHashings = listOf(),
          randomSeed = "",
          event = labelerEvent {}
        )
      }
    assertTrue(exception.message!!.contains("No column matcher is set"))
  }
}
