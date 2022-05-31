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

package org.wfanet.virtualpeople.core.labeler

import org.wfanet.measurement.common.loadLibraryFromResource
import org.wfanet.measurement.common.wrapJniException
import org.wfanet.virtualpeople.LabelerSwig
import org.wfanet.virtualpeople.common.LabelerInput
import org.wfanet.virtualpeople.common.LabelerOutput

private const val SWIG_PREFIX: String = "/main/swig/wfa/virtual_people/core/labeler"

/** Used to label events with VID. */
class JniLabeler : Labeler {

  init {
    loadLibraryFromResource("labeler", SWIG_PREFIX)
  }

  /** Labels a list of inputs. */
  override fun label(modelPath: String, inputs: List<LabelerInput>): List<LabelerOutput> {
    val request = labelEventsRequest {
      this.modelPath = modelPath
      this.inputs += inputs
    }
    val response = wrapJniException {
      LabelEventsResponse.parseFrom(LabelerSwig.labelEventsWrapper(request.toByteArray()))
    }
    return response.outputsList
  }
}
