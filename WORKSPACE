load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Common-cpp
http_archive(
    name = "wfa_common_cpp",
    sha256 = "60e9c808d55d14be65347cab008b8bd4f8e2dd8186141609995333bc75fc08ce",
    strip_prefix = "common-cpp-0.8.0",
    url = "https://github.com/world-federation-of-advertisers/common-cpp/archive/refs/tags/v0.8.0.tar.gz",
)

load("@wfa_common_cpp//build:common_cpp_repositories.bzl", "common_cpp_repositories")

common_cpp_repositories()

load("@wfa_common_cpp//build:common_cpp_deps.bzl", "common_cpp_deps")

common_cpp_deps()

# Virtual-people-common
http_archive(
    name = "virtual_people_common",
    sha256 = "878da77d98f5216951c90e72a6da5f0cd5ed5cdc8e862c0934f7cf7d200e9ce3",
    strip_prefix = "virtual-people-common-7268767b7034bdf7ad4e5282cb892d0618cd1030",
    url = "https://github.com/world-federation-of-advertisers/virtual-people-common/archive/7268767b7034bdf7ad4e5282cb892d0618cd1030.tar.gz",
)
