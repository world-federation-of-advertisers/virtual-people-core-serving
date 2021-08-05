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

# Virtual-people-common
http_archive(
    name = "virtual_people_common",
    sha256 = "fa100fb0acaeffc6192a0c26f0c4ee96ba269de2d004797330d71985777e4906",
    strip_prefix = "virtual-people-common-77883e639ad22431bca5eeaffd08fbf5e320bb68",
    url = "https://github.com/world-federation-of-advertisers/virtual-people-common/archive/77883e639ad22431bca5eeaffd08fbf5e320bb68.tar.gz",
)
