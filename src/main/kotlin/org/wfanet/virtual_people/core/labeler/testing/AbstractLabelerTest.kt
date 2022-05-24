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

package org.wfanet.virtualpeople.core.labeler.testing

import com.google.common.truth.Truth.assertThat
import java.nio.file.Path
import java.nio.file.Paths
import org.junit.Test
import org.wfanet.measurement.common.getRuntimePath
import org.wfanet.measurement.common.parseTextProto
import org.wfanet.virtualpeople.common.CompiledNode
import org.wfanet.virtualpeople.common.LabelerInput
import org.wfanet.virtualpeople.common.LabelerOutput
import org.wfanet.virtualpeople.common.compiledNode
import org.wfanet.virtualpeople.common.labelerInput
import org.wfanet.virtualpeople.common.labelerOutput
import org.wfanet.virtualpeople.core.labeler.Labeler

private val DATA_PATH: Path =
  getRuntimePath(
    Paths.get(
      "wfa_virtual_people_core_serving",
      "src",
      "test",
      "cc",
      "wfa",
      "virtual_people",
      "core",
      "labeler",
      "test_data"
    )
  )!!

private const val MODEL_PATH = "toy_model.textproto"
private val INPUT_FILES =
  listOf(
    "labeler_input_01.textproto",
    "labeler_input_02.textproto",
    "labeler_input_03.textproto",
    "labeler_input_04.textproto",
    "labeler_input_05.textproto",
    "labeler_input_06.textproto",
    "labeler_input_07.textproto",
    "labeler_input_08.textproto",
    "labeler_input_09.textproto",
    "labeler_input_10.textproto",
    "labeler_input_11.textproto",
    "labeler_input_12.textproto",
  )
private val OUTPUT_FILES =
  listOf(
    "labeler_output_01.textproto",
    "labeler_output_02.textproto",
    "labeler_output_03.textproto",
    "labeler_output_04.textproto",
    "labeler_output_05.textproto",
    "labeler_output_06.textproto",
    "labeler_output_07.textproto",
    "labeler_output_08.textproto",
    "labeler_output_09.textproto",
    "labeler_output_10.textproto",
    "labeler_output_11.textproto",
    "labeler_output_12.textproto",
  )

/** Abstract base class for testing implementations of [Labeler]. */
abstract class AbstractLabelerTest {
  abstract val labeler: Labeler
  val inputs: List<LabelerInput> by lazy {
    INPUT_FILES.map { file -> parseTextProto(DATA_PATH.resolve(file).toFile(), labelerInput {}) }
  }
  val expectedOutputs: List<LabelerOutput> by lazy {
    OUTPUT_FILES.map { file -> parseTextProto(DATA_PATH.resolve(file).toFile(), labelerOutput {}) }
  }
  val nodes: List<CompiledNode> by lazy {
    listOf(parseTextProto(DATA_PATH.resolve(MODEL_PATH).toFile(), compiledNode {}))
  }

  @Test
  fun testLabeler() {

    val outputs = labeler.label(nodes, inputs)
    assertThat(outputs).isEqualTo(expectedOutputs)
  }
}
