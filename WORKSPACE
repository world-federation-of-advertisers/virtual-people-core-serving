load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "com_google_googletest",
    urls = ["https://github.com/google/googletest/archive/release-1.10.0.tar.gz"],
    sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
    strip_prefix = "googletest-release-1.10.0",
)

http_archive(
    name = "com_google_protobuf",
    sha256 = "355cf346e6988fd219ff7b18e6e68a742aaef09a400a0cf2860e7841468a12ac",
    strip_prefix = "protobuf-3.15.7",
    urls = ["https://github.com/protocolbuffers/protobuf/releases/download/v3.15.7/protobuf-all-3.15.7.tar.gz"],
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

http_archive(
    name = "com_google_absl",
    sha256 = "dd7db6815204c2a62a2160e32c55e97113b0a0178b2f090d6bab5ce36111db4b",
    strip_prefix = "abseil-cpp-20210324.0",
    urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20210324.0.tar.gz"],
)

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")

new_git_repository(
  name = "com_google_farmhash",
  remote = "https://github.com/google/farmhash",
  commit = "0d859a811870d10f53a594927d0d0b97573ad06d",
  build_file = "farmhash.BUILD",
)

git_repository(
  name = "virtual_people_common",
  remote = "https://github.com/world-federation-of-advertisers/virtual-people-common",
  commit = "286a38bdc42e10e1fc49b53f84f8a012cef59d36",
)

git_repository(
    name = "cross_media_measurement",
    remote = "https://github.com/world-federation-of-advertisers/cross-media-measurement",
    commit = "a4863588aa84c965e6ec0d0b1d6e535b0d86d388",
    repo_mapping = {"@googletest": "@com_google_googletest"}
)
