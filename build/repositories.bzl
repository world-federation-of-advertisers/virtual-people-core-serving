# Copyright 2021 The Cross-Media Measurement Authors
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
Step 1 of configuring WORKSPACE: adds direct deps.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def virtual_people_core_serving_repositories():
    """Imports all direct dependencies for virtual_people."""

    native.local_repository(
        name = "wfa_common_cpp",
        path = "/development/common-cpp",
    )

    # TODO Update to version before merging
    http_archive(
        name = "wfa_common_cpp2",
        sha256 = "885f286e73e060adc94ac6b2784a125fc6dcd554dd8c6155db73d4b6c368b1e6",
        strip_prefix = "common-cpp-4a32da909004a1d6a53fba50f8c5e44d34d5131b",
        url = "https://github.com/world-federation-of-advertisers/common-cpp/archive/4a32da909004a1d6a53fba50f8c5e44d34d5131b.tar.gz",
    )

    # TODO Update to version before merging
    http_archive(
        name = "wfa_common_jvm",
        sha256 = "0bec09b1edeb4af9227b5faa6f5d54da55fc10a33bf89b5d30d9facf17838709",
        strip_prefix = "common-jvm-f21052883cfd13f6d4a014b94afdbefa085bdb10",
        url = "https://github.com/world-federation-of-advertisers/common-jvm/archive/f21052883cfd13f6d4a014b94afdbefa085bdb10.tar.gz",
    )

    http_archive(
        name = "wfa_rules_swig",
        sha256 = "34c15134d7293fc38df6ed254b55ee912c7479c396178b7f6499b7e5351aeeec",
        strip_prefix = "rules_swig-653d1bdcec85a9373df69920f35961150cf4b1b6",
        url = "https://github.com/world-federation-of-advertisers/rules_swig/archive/653d1bdcec85a9373df69920f35961150cf4b1b6.tar.gz",
    )

    http_archive(
        name = "rules_pkg",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.6.0/rules_pkg-0.6.0.tar.gz",
            "https://github.com/bazelbuild/rules_pkg/releases/download/0.6.0/rules_pkg-0.6.0.tar.gz",
        ],
        sha256 = "62eeb544ff1ef41d786e329e1536c1d541bb9bcad27ae984d57f18f314018e66",
    )

    # TODO Update to version before merging
    http_archive(
        name = "wfa_virtual_people_common",
        sha256 = "3744dc1d19a0912c33c513ce16da53692b4a5d20b47c7a70d61431bcb97b0a4e",
        strip_prefix = "virtual-people-common-dd1896d1a4d07f4d49d595abf3c84d7caa179adc",
        url = "https://github.com/world-federation-of-advertisers/virtual-people-common/archive/dd1896d1a4d07f4d49d595abf3c84d7caa179adc.tar.gz",
        repo_mapping = {
            # Remove this once wfa_virtual_people_common is fixed
            "@com_google_protobuf": "@com_github_protocolbuffers_protobuf",
        },
    )
