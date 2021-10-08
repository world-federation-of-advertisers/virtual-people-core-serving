load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Common-cpp
http_archive(
    name = "wfa_common_cpp",
    sha256 = "2c30e218a595483a9d0f2ca7117bc40cbc522cf513b2b8ee9db4570ffd35027f",
    strip_prefix = "common-cpp-0.3.0",
    url = "https://github.com/world-federation-of-advertisers/common-cpp/archive/refs/tags/v0.3.0.tar.gz",
)

load("@wfa_common_cpp//build:common_cpp_repositories.bzl", "common_cpp_repositories")

common_cpp_repositories()

load("@wfa_common_cpp//build:common_cpp_deps.bzl", "common_cpp_deps")

common_cpp_deps()

# Virtual-people-common
http_archive(
    name = "virtual_people_common",
    sha256 = "7fcc40826493f824b5ec083822a9e430ddfbf8f235171149ae9a1c6896984d88",
    strip_prefix = "virtual-people-common-562566aa13fb67116f034890fb3b589eeae0aa89",
    url = "https://github.com/world-federation-of-advertisers/virtual-people-common/archive/562566aa13fb67116f034890fb3b589eeae0aa89.tar.gz",
)
