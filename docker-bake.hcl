variable "SIGNALROUTE_TAG" {
  default = "local"
}

group "default" {
  targets = ["fallback"]
}

group "adapter" {
  targets = ["adapter-scaffold"]
}

target "fallback" {
  context = "."
  dockerfile = "Dockerfile"
  tags = ["signalroute:${SIGNALROUTE_TAG}"]
}

target "adapter-scaffold" {
  context = "."
  dockerfile = "Dockerfile.adapters"
  target = "runtime"
  tags = ["signalroute:adapter-scaffold"]
  args = {
    SR_DEPENDENCY_PROVIDER = "system"
    SR_BUILD_TESTS = "OFF"
    SR_ENABLE_PROTOBUF = "OFF"
    SR_ENABLE_GRPC = "OFF"
    SR_ENABLE_REAL_H3 = "OFF"
    SR_ENABLE_REAL_REDIS = "OFF"
    SR_ENABLE_REAL_POSTGIS = "OFF"
    SR_ENABLE_REAL_KAFKA = "OFF"
    SR_ENABLE_PROMETHEUS = "OFF"
    SR_ENABLE_TOMLPLUSPLUS = "OFF"
  }
}

target "adapter-protobuf" {
  inherits = ["adapter-scaffold"]
  tags = ["signalroute:adapter-protobuf"]
  args = {
    SR_ADAPTER_APT_PACKAGES = "libprotobuf-dev protobuf-compiler"
    SR_ADAPTER_RUNTIME_APT_PACKAGES = "libprotobuf32t64"
    SR_ENABLE_PROTOBUF = "ON"
  }
}

target "adapter-kafka" {
  inherits = ["adapter-scaffold"]
  tags = ["signalroute:adapter-kafka"]
  args = {
    SR_ADAPTER_APT_PACKAGES = "librdkafka-dev"
    SR_ADAPTER_RUNTIME_APT_PACKAGES = "librdkafka++1"
    SR_ENABLE_REAL_KAFKA = "ON"
  }
}

target "integration-ingestion" {
  inherits = ["adapter-scaffold"]
  target = "adapter-build"
  tags = ["signalroute:integration-ingestion"]
  args = {
    SR_ADAPTER_APT_PACKAGES = "librdkafka-dev libprotobuf-dev protobuf-compiler"
    SR_BUILD_TESTS = "ON"
    SR_BUILD_INTEGRATION_TESTS = "ON"
    SR_ADAPTER_BUILD_TARGETS = "test_ingestion_pipeline"
    SR_ENABLE_PROTOBUF = "ON"
    SR_ENABLE_REAL_KAFKA = "ON"
  }
}
