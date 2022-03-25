load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Common-cpp
http_archive(
    name = "wfa_common_cpp",
    sha256 = "a964e147d4516ec40560531ad5fd654bd4aef37fc05c58d44ad1749b2dc466dc",
    strip_prefix = "common-cpp-0.6.0",
    url = "https://github.com/world-federation-of-advertisers/common-cpp/archive/refs/tags/v0.6.0.tar.gz",
)

load("@wfa_common_cpp//build:common_cpp_repositories.bzl", "common_cpp_repositories")

common_cpp_repositories()

load("@wfa_common_cpp//build:common_cpp_deps.bzl", "common_cpp_deps")

common_cpp_deps()

# Virtual-people-common
http_archive(
    name = "virtual_people_common",
    sha256 = "12e42a3d449771090292fbac321532b794b495607493b870a52101c7d356c7bd",
    strip_prefix = "virtual-people-common-8dd8d59f0cc9ee5e4c446031790973a6cb51970e",
    url = "https://github.com/world-federation-of-advertisers/virtual-people-common/archive/8dd8d59f0cc9ee5e4c446031790973a6cb51970e.tar.gz",
)
