# Copyright 2022 The Cross-Media Measurement Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Adds external repos necessary for virtual_people_core_serving.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

def virtual_people_core_serving_repositories():
    """Imports all direct dependencies for virtual_people_core_serving."""
    http_archive(
        name = "wfa_common_cpp",
        sha256 = "60e9c808d55d14be65347cab008b8bd4f8e2dd8186141609995333bc75fc08ce",
        strip_prefix = "common-cpp-0.8.0",
        url = "https://github.com/world-federation-of-advertisers/common-cpp/archive/refs/tags/v0.8.0.tar.gz",
    )

    http_archive(
        name = "wfa_common_jvm",
        sha256 = "ad623ee3b1893b47fc6c86d6b1c90ea1f46a44bdf502a1847518f6769597c5cf",
        strip_prefix = "common-jvm-0.45.0",
        url = "https://github.com/world-federation-of-advertisers/common-jvm/archive/refs/tags/v0.45.0.tar.gz",
    )

    # TODO(@wangyaopw): switch to version based http_archive when the common library implementation is completed
    git_repository(
        name = "virtual_people_common",
        commit = "fd55a91342608f8ac06ce0ffe96a017f86e7b3c6",
        remote = "https://github.com/world-federation-of-advertisers/virtual-people-common.git",
    )



