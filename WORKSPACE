workspace(name = "wfa_virtual_people_core_serving")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

load("//build:repositories.bzl", "virtual_people_core_serving_repositories")

virtual_people_core_serving_repositories()

load("@wfa_common_jvm//build:common_jvm_repositories.bzl", "common_jvm_repositories")

common_jvm_repositories()

load("@wfa_common_jvm//build:common_jvm_deps.bzl", "common_jvm_deps")

common_jvm_deps()

load("@wfa_common_cpp//build:common_cpp_repositories.bzl", "common_cpp_repositories")

common_cpp_repositories()

load("@wfa_common_cpp//build:common_cpp_deps.bzl", "common_cpp_deps")

common_cpp_deps()

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

# Maven
load("@wfa_common_jvm//build:common_jvm_maven.bzl", "COMMON_JVM_MAVEN_OVERRIDE_TARGETS", "common_jvm_maven_artifacts")
load("@rules_jvm_external//:defs.bzl", "maven_install")

# TODO: Get these from common-jvm
GRPC_JAVA_VERSION = "1.43.2"

KOTLIN_VERSION = "1.4.31"

maven_install(
    artifacts = common_jvm_maven_artifacts(),
    fetch_sources = True,
    generate_compat_repositories = True,
    override_targets = COMMON_JVM_MAVEN_OVERRIDE_TARGETS,
    repositories = [
        "https://repo.maven.apache.org/maven2/",
    ],
)

maven_install(
    name = "maven_export",
    artifacts = common_jvm_maven_artifacts() + [
        "io.grpc:grpc-kotlin-stub:1.2.0",
        "io.grpc:grpc-netty:" + GRPC_JAVA_VERSION,
        "io.grpc:grpc-services:" + GRPC_JAVA_VERSION,
        "org.jetbrains.kotlin:kotlin-reflect:" + KOTLIN_VERSION,
        "org.jetbrains.kotlin:kotlin-stdlib-jdk7:" + KOTLIN_VERSION,
        "org.jetbrains.kotlin:kotlin-test:" + KOTLIN_VERSION,
    ],
    fetch_sources = True,
    generate_compat_repositories = True,
    override_targets = COMMON_JVM_MAVEN_OVERRIDE_TARGETS,
    repositories = [
        "https://repo.maven.apache.org/maven2/",
    ],
)
