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

package org.wfanet.virtualpeople.core.selector

import java.io.File
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import org.wfanet.measurement.api.v2alpha.modelLine
import org.wfanet.measurement.api.v2alpha.modelRollout
import org.wfanet.measurement.common.parseTextProto
import org.wfanet.virtualpeople.common.labelerInput
import org.wfanet.virtualpeople.common.profileInfo
import org.wfanet.virtualpeople.common.userInfo


private const val TEXTPROTO_PATH = "src/test/proto/wfa/virtual_people/core/selector/test_data"

@RunWith(JUnit4::class)
class VidModelSelectorTest {

  @Test
  fun `VidModelSelector object creation fails when ModelRollout is not parented by the provided ModelLine`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_02.textproto").bufferedReader(), modelLine {})
    val modelRollout =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})

    val exception =
      assertFailsWith<IllegalArgumentException> { VidModelSelector(modelLine, listOf(modelRollout)) }

    assertEquals(exception.message, "ModelRollouts must be parented by the provided ModelLine.")

  }

  @Test
  fun `getModelRelease returns null when model line is not yet active`() {
    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val vidModelSelector = VidModelSelector(modelLine, listOf())
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 900_000_000_000_000L
    })
    assertNull(modelRelease)
  }

  @Test
  fun `getModelRelease returns null when model line is no longer active`() {
    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val vidModelSelector = VidModelSelector(modelLine, listOf())
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 2_100_000_000_000_000L
    })
    assertNull(modelRelease)
  }

  @Test
  fun `getModelRelease returns null when rollouts list is empty`() {
    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val vidModelSelector = VidModelSelector(modelLine, listOf())
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_200_000_000_000_000L
    })
    assertNull(modelRelease)
  }

  @Test
  fun `getModelRelease returns null when event time precedes rollout period start time`() {
    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout1))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_050_000_000_000_000L
    })
    assertNull(modelRelease)
  }

  @Test
  fun `getModelRelease returns correct ModelRelease with single ModelRollout with rollout period`() {
    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout1))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_200_000_000_000_000L
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_01")
  }

  @Test
  fun `getModelRelease returns correct ModelRelease with single ModelRollout without rollout period`() {
    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRolloutWithoutRolloutPeriod2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_without_rollout_period_02.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRolloutWithoutRolloutPeriod2))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_200_000_000_000_000L
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_without_rollout_period_02")
  }


  @Test
  fun `getModelRelease returns correct ModelRelease when rollouts stack is (R1, R2(0,0 - 1,1)) and event time is after R2 rollout period end time`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRollout1))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_800_000_000_000_000L
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_02")

  }

  @Test
  fun `getModelRelease returns correct ModelRelease when rollouts stack is (R2(0,0 - 0,80), R1(0,80 - 1,1)) and reduceEventId is 0,75`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRollout1))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_620_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "abc@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_02")

  }

  @Test
  fun `getModelRelease returns correct ModelRelease when rollouts stack is (R2(0,0 - 0,50), R1(0,50 - 1,1)) and reduceEventId is 0,75`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRollout1))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_500_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "abc@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_01")

  }

  @Test
  fun `getModelRelease returns correct ModelRelease when rollouts stack is (R2(0,0 - 0,50), R1(0,50 - 1,1)) and reduceEventId is 0,36`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRollout1))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_500_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "xyz@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_02")

  }

  @Test
  fun `getModelRelease returns correct ModelRelease when event happens during rollout_01's rollout period but before rollout_02's rollout period start time`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRollout1))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_200_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "abc@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_01")

  }

  @Test
  fun `getModelRelease returns same ModelRelease if invoked twice with same labelerInput`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRollout1))
    val modelRelease1 = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_500_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "xyz@mail.com"
        }
      }
    })
    assertEquals(modelRelease1, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_02")

    val modelRelease2 = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_500_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "xyz@mail.com"
        }
      }
    })
    assertEquals(modelRelease2, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_02")

  }

  @Test
  fun `getModelRelease returns correct ModelRelease when rollouts stack is (R3(0,0 - 0,12), R2(0,12 - 0,45), R1(0,45 - 1,1)) and reduceEventId is 0,36`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val modelRollout3 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_03.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRollout1, modelRollout3))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_450_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "xyz@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_02")

  }

  @Test
  fun `getModelRelease returns correct ModelRelease when rollouts stack is (R3(0,0 - 0,12), R2(0,12 - 0,45), R1(0,45 - 1,1)) and reduceEventId is 0,75`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val modelRollout3 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_03.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRollout1, modelRollout3))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_450_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "abc@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_01")

  }

  @Test
  fun `getModelRelease excludes rollouts that precedes a rollout without rollout period interval`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_01.textproto").bufferedReader(), modelRollout {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val modelRollout3 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_03.textproto").bufferedReader(), modelRollout {})
    val modelRolloutWithoutRolloutPeriod1 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_without_rollout_period_01.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRollout1, modelRollout3, modelRolloutWithoutRolloutPeriod1))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_450_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "abc@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_without_rollout_period_01")

  }

  @Test
  fun `getModelRelease blocks rollout expansion when even is after rollout freeze time and reduceEventId is 0,36`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val modelRolloutFreeze =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_freeze_time_01.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRolloutFreeze))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_900_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "xyz@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_freeze_time_01")

  }

  @Test
  fun `getModelRelease blocks rollout expansion when even is after rollout freeze time and reduceEventId is 0,75`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val modelRolloutFreeze =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_freeze_time_01.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRolloutFreeze))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_900_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "abc@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_02")

  }

  @Test
  fun `getModelRelease returns correct ModelRelease when rollouts stack is (R_Freeze(0,0 - 0,42), R2(0,42 - 1,1)) and reduceEventId is 0,75`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val modelRolloutFreeze =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_freeze_time_01.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRolloutFreeze))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_900_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "abc@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_02")

  }

  @Test
  fun `getModelRelease returns correct ModelRelease when rollouts stack is (R_Freeze(0,0 - 0,42), R2(0,42 - 1,1)) and reduceEventId is 0,36`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRollout2 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_02.textproto").bufferedReader(), modelRollout {})
    val modelRolloutFreeze =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_freeze_time_01.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRollout2, modelRolloutFreeze))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_570_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "xyz@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_freeze_time_01")

  }

  @Test
  fun `getModelRelease returns correct ModelRelease when ModelRollout without rollout period replaces ModelRollout with freeze time and reduceEventId is 0,36`() {

    val modelLine =
      parseTextProto(File("$TEXTPROTO_PATH/model_line_01.textproto").bufferedReader(), modelLine {})
    val modelRolloutWithoutRolloutPeriod3 =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_without_rollout_period_03.textproto").bufferedReader(), modelRollout {})
    val modelRolloutFreeze =
      parseTextProto(File("$TEXTPROTO_PATH/model_rollout_freeze_time_01.textproto").bufferedReader(), modelRollout {})
    val vidModelSelector = VidModelSelector(modelLine, listOf(modelRolloutWithoutRolloutPeriod3, modelRolloutFreeze))
    val modelRelease = vidModelSelector.getModelRelease(labelerInput {
      timestampUsec = 1_600_000_000_000_000L
      profileInfo = profileInfo {
        emailUserInfo = userInfo {
          userId = "xyz@mail.com"
        }
      }
    })
    assertEquals(modelRelease, "modelProviders/AAAAAAAAAHs/modelSuites/AAAAAAAAAHs/modelReleases/rollout_without_rollout_period_03")

  }

}
