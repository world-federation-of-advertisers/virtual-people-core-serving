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

package src.main.kotlin.org.wfanet.virtualpeople.core.selector

import org.wfanet.measurement.api.v2alpha.ModelLine
import org.wfanet.measurement.api.v2alpha.ModelRollout
import org.wfanet.measurement.api.v2alpha.ModelRelease
import src.main.kotlin.org.wfanet.virtualpeople.core.selector.cache.LRUCache
import org.wfanet.virtualpeople.common.*

const val CACHE_SIZE = 60

class VidModelSelector(private val modelLine: ModelLine, private val rollouts: List<ModelRollout>) {

  private val lruCache: LRUCache<Int, Array<Triple<Double, Double, ModelRelease>>>

  init {
      lruCache = LRUCache<Int, Array<Triple<Double,Double,ModelRelease>>>(CACHE_SIZE)
  }

  fun getModelRelease(labelerInput: LabelerInput): ModelRelease? {
    return null
  }

}
