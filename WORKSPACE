load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Common-cpp
http_archive(
    name = "wfa_common_cpp",
    sha256 = "aac2fa570a63c974094e09a5c92585a4e992b429c658057d187f46381be3ce50",
    strip_prefix = "common-cpp-0.1.0",
    url = "https://github.com/world-federation-of-advertisers/common-cpp/archive/refs/tags/v0.1.0.tar.gz",
)

load("@wfa_common_cpp//build:common_cpp_repositories.bzl", "common_cpp_repositories")

common_cpp_repositories()

load("@wfa_common_cpp//build:common_cpp_deps.bzl", "common_cpp_deps")

common_cpp_deps()

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

git_repository(
    name = "virtual_people_common",
    # TODO(@tcsnfkx): Fix this after https://github.com/world-federation-of-advertisers/virtual-people-common/pull/20 is merged.
    branch = "tcsnfkx-common-cpp-migrate",
    # commit = "36f58cfc29901a3110df585faba206964b96e4a1",
    remote = "https://github.com/world-federation-of-advertisers/virtual-people-common",
)
