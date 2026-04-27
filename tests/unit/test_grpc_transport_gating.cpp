#include <cassert>
#include <iostream>

int main() {
    std::cout << "test_grpc_transport_gating:\n";
#if SIGNALROUTE_HAS_GRPC
    static_assert(SIGNALROUTE_HAS_PROTOBUF, "gRPC builds must also enable protobuf");
    assert(SIGNALROUTE_HAS_PROTOBUF == 1);
#else
    assert(SIGNALROUTE_HAS_GRPC == 0);
#endif
    std::cout << "All gRPC transport gating tests passed.\n";
    return 0;
}
