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

target "adapter-h3" {
  inherits = ["adapter-scaffold"]
  tags = ["signalroute:adapter-h3"]
  args = {
    SR_ADAPTER_APT_PACKAGES = "libh3-dev"
    SR_ADAPTER_RUNTIME_APT_PACKAGES = "libh3-1"
    SR_ENABLE_REAL_H3 = "ON"
  }
}

target "adapter-h3-test" {
  inherits = ["adapter-scaffold"]
  target = "adapter-build"
  tags = ["signalroute:adapter-h3-test"]
  args = {
    SR_ADAPTER_APT_PACKAGES = "libh3-dev"
    SR_BUILD_TESTS = "ON"
    SR_ADAPTER_BUILD_TARGETS = "test_h3_index"
    SR_ENABLE_REAL_H3 = "ON"
  }
}

target "adapter-redis" {
  inherits = ["adapter-scaffold"]
  tags = ["signalroute:adapter-redis"]
  args = {
    SR_ADAPTER_APT_PACKAGES = "libhiredis-dev"
    SR_ADAPTER_RUNTIME_APT_PACKAGES = "libhiredis1.1.0"
    SR_ENABLE_REAL_REDIS = "ON"
  }
}

target "adapter-redis-test" {
  inherits = ["adapter-scaffold"]
  target = "adapter-build"
  tags = ["signalroute:adapter-redis-test"]
  args = {
    SR_ADAPTER_APT_PACKAGES = "libhiredis-dev"
    SR_BUILD_TESTS = "ON"
    SR_ADAPTER_BUILD_TARGETS = "test_redis_client"
    SR_ENABLE_REAL_REDIS = "ON"
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
