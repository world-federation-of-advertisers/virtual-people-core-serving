/*
 * Copyright 2023 The Cross-Media Measurement Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.wfanet.virtualpeople.core.selector.resourcekey

import org.wfanet.measurement.common.ResourceNameParser

const val MODEL_PROVIDER = "model_provider"
const val MODEL_SUITE = "model_suite"
const val MODEL_LINE = "model_line"

private val parser =
  ResourceNameParser(
    "modelProviders/{model_provider}/modelSuites/{model_suite}/modelLines/{model_line}"
  )

data class ModelLineKey(
  val modelProviderId: String,
  val modelSuiteId: String,
  val modelLineId: String
) {
  companion object {
    fun fromName(resourceName: String): ModelLineKey? {
      return parser.parseIdSegments(resourceName)?.let {
        ModelLineKey(it.getValue(MODEL_PROVIDER), it.getValue(MODEL_SUITE), it.getValue(MODEL_LINE))
      }
    }
  }
}
