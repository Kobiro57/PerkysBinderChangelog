#include "../include/ServerCheckpointRPCAdapter.h"
#include "../include/RakHook/include/RakNet/BitStream.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] ServerCheckpointRPC: %s (line %d)\n", #value, __LINE__); return; } } while (0)

constexpr unsigned char kSetCheckpoint = 107;
constexpr unsigned char kDisableCheckpoint = 37;

struct PayloadSnapshot {
    int readOffset;
    int usedBits;
    std::vector<unsigned char> bytes;
};

PayloadSnapshot Snapshot(const RakNet::BitStream& payload) {
    return {payload.GetReadOffset(), payload.GetNumberOfBitsUsed(),
        std::vector<unsigned char>(payload.GetData(), payload.GetData() + payload.GetNumberOfBytesUsed())};
}

bool Unchanged(const RakNet::BitStream& payload, const PayloadSnapshot& before) {
    return payload.GetReadOffset() == before.readOffset
        && payload.GetNumberOfBitsUsed() == before.usedBits
        && std::vector<unsigned char>(payload.GetData(),
            payload.GetData() + payload.GetNumberOfBytesUsed()) == before.bytes;
}

void WriteCheckpoint(RakNet::BitStream& payload, float x = 1583.9396f, float y = 33.5867f,
                     float z = 26.9868f, float radius = 3.0f) {
    payload.Write(x);
    payload.Write(y);
    payload.Write(z);
    payload.Write(radius);
}

void TestSetCheckpointParsesEveryBitAlignmentAndDoesNotMutateTheRpc() {
    keybinder::ServerCheckpointRPCAdapter adapter;
    std::vector<keybinder::ServerCheckpointEvent> events;
    adapter.SetCallback([&](const auto& event) { events.push_back(event); });
    CHECK(adapter.Initialize());
    for (int prefixBits = 0; prefixBits < 8; ++prefixBits) {
        RakNet::BitStream payload;
        for (int bit = 0; bit < prefixBits; ++bit) payload.Write(bit % 2 == 0);
        WriteCheckpoint(payload, -2354.125f, 1420.5f, 41.75f, 4.5f);
        payload.Write(static_cast<unsigned short>(0xCAFE));
        payload.Write(false);
        payload.SetReadOffset(prefixBits);
        const auto before = Snapshot(payload);
        CHECK(adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
        CHECK(Unchanged(payload, before));
        CHECK(events.size() == static_cast<std::size_t>(prefixBits + 1));
        const auto& event = events.back();
        CHECK(event.enabled);
        CHECK(event.x == -2354.125f && event.y == 1420.5f && event.z == 41.75f && event.radius == 4.5f);
    }
}

void TestEveryTruncatedPayloadRestoresTheOriginalReadPositionAndBytes() {
    keybinder::ServerCheckpointRPCAdapter adapter;
    int delivered = 0;
    adapter.SetCallback([&](const auto&) { ++delivered; });
    CHECK(adapter.Initialize());
    CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, nullptr));
    RakNet::BitStream complete;
    complete.Write(true); complete.Write(false); complete.Write(true);
    WriteCheckpoint(complete);
    for (int bodyBits = 0; bodyBits < 128; ++bodyBits) {
        RakNet::BitStream truncated(complete.GetData(), complete.GetNumberOfBytesUsed(), true);
        truncated.SetWriteOffset(3 + bodyBits);
        truncated.SetReadOffset(3);
        const auto before = Snapshot(truncated);
        CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &truncated));
        CHECK(Unchanged(truncated, before));
        CHECK(delivered == 0);
    }
    complete.SetReadOffset(complete.GetNumberOfBitsUsed());
    const auto consumed = Snapshot(complete);
    CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &complete));
    CHECK(Unchanged(complete, consumed) && delivered == 0);
}

void TestNanInfinityInvalidCoordinatesAndRadiusNeverReachTheCallback() {
    keybinder::ServerCheckpointRPCAdapter adapter;
    int delivered = 0;
    adapter.SetCallback([&](const auto&) { ++delivered; });
    CHECK(adapter.Initialize());
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (int field = 0; field < 4; ++field) {
        for (float invalid : {nan, inf, -inf}) {
            float values[] = {100.0f, -200.0f, 30.0f, 3.0f};
            values[field] = invalid;
            RakNet::BitStream payload;
            payload.Write(true);
            WriteCheckpoint(payload, values[0], values[1], values[2], values[3]);
            payload.SetReadOffset(1);
            const auto before = Snapshot(payload);
            CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
            CHECK(Unchanged(payload, before));
        }
    }
    for (int field = 0; field < 3; ++field) {
        for (float outside : {std::nextafter(100000.0f, inf), std::nextafter(-100000.0f, -inf)}) {
            float values[] = {100.0f, -200.0f, 30.0f, 3.0f};
            values[field] = outside;
            RakNet::BitStream payload;
            WriteCheckpoint(payload, values[0], values[1], values[2], values[3]);
            const auto before = Snapshot(payload);
            CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
            CHECK(Unchanged(payload, before));
        }
    }
    for (float invalid : {0.0f, -0.0f, -1.0f, std::nextafter(10000.0f, inf)}) {
        RakNet::BitStream payload;
        WriteCheckpoint(payload, 0.0f, 0.0f, 0.0f, invalid);
        const auto before = Snapshot(payload);
        CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
        CHECK(Unchanged(payload, before));
    }
    CHECK(delivered == 0);
}

void TestCoordinateAndRadiusBoundariesRemainValidAndReadOnly() {
    keybinder::ServerCheckpointRPCAdapter adapter;
    std::vector<keybinder::ServerCheckpointEvent> events;
    adapter.SetCallback([&](const auto& event) { events.push_back(event); });
    CHECK(adapter.Initialize());
    for (float radius : {std::numeric_limits<float>::min(), 1.0f, 10000.0f}) {
        RakNet::BitStream payload;
        WriteCheckpoint(payload, -100000.0f, 100000.0f, -100000.0f, radius);
        const auto before = Snapshot(payload);
        CHECK(adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
        CHECK(Unchanged(payload, before));
        CHECK(events.back().enabled && events.back().x == -100000.0f
            && events.back().y == 100000.0f && events.back().z == -100000.0f
            && events.back().radius == radius);
    }
    CHECK(events.size() == 3);
}

void TestUnrelatedRpcIdsAreIgnored() {
    keybinder::ServerCheckpointRPCAdapter adapter;
    int delivered = 0;
    adapter.SetCallback([&](const auto&) { ++delivered; });
    CHECK(adapter.Initialize());
    RakNet::BitStream payload;
    payload.Write(true); payload.Write(true);
    WriteCheckpoint(payload);
    payload.SetReadOffset(2);
    const auto before = Snapshot(payload);
    for (unsigned char rpc : {0, 1, 36, 40, 61, 106, 108, 255}) {
        CHECK(!adapter.ProcessRpcForTesting(rpc, &payload));
        CHECK(Unchanged(payload, before));
        CHECK(!adapter.ProcessRpcForTesting(rpc, nullptr));
    }
    CHECK(delivered == 0);
}

void TestRacingCheckpointLayoutAndDisablePreservePayload() {
    keybinder::ServerCheckpointRPCAdapter adapter;
    std::vector<keybinder::ServerCheckpointEvent> events;
    adapter.SetCallback([&](const auto& event) { events.push_back(event); });
    CHECK(adapter.Initialize());
    for (int prefix = 0; prefix < 8; ++prefix) {
        RakNet::BitStream payload;
        for (int bit = 0; bit < prefix; ++bit) payload.Write(true);
        payload.Write(static_cast<unsigned char>(1));
        payload.Write(1927.0f); payload.Write(168.0f); payload.Write(40.0f);
        payload.Write(0.0f); payload.Write(0.0f); payload.Write(0.0f);
        payload.Write(4.0f);
        payload.SetReadOffset(prefix);
        const auto before = Snapshot(payload);
        CHECK(adapter.ProcessRpcForTesting(38, &payload));
        CHECK(Unchanged(payload, before));
        CHECK(events.back().enabled && events.back().racing);
        CHECK(events.back().x == 1927.0f && events.back().y == 168.0f
            && events.back().z == 40.0f && events.back().radius == 4.0f);
        const std::size_t count = events.size();
        for (int body = 0; body < 232; ++body) {
            RakNet::BitStream partial(payload.GetData(), payload.GetNumberOfBytesUsed(), true);
            partial.SetWriteOffset(prefix + body); partial.SetReadOffset(prefix);
            const auto saved = Snapshot(partial);
            CHECK(!adapter.ProcessRpcForTesting(38, &partial));
            CHECK(Unchanged(partial, saved));
            CHECK(events.size() == count);
        }
    }
    CHECK(adapter.ProcessRpcForTesting(39, nullptr));
    CHECK(!events.back().enabled && events.back().racing);
    CHECK(adapter.ProcessRpcForTesting(37, nullptr));
    CHECK(!events.back().enabled && !events.back().racing);
}

void TestDisableCheckpointAllowsNullOrUnrelatedPayloadWithoutReadingIt() {
    keybinder::ServerCheckpointRPCAdapter adapter;
    std::vector<keybinder::ServerCheckpointEvent> events;
    adapter.SetCallback([&](const auto& event) { events.push_back(event); });
    CHECK(adapter.Initialize());
    CHECK(adapter.ProcessRpcForTesting(kDisableCheckpoint, nullptr));
    CHECK(events.size() == 1 && !events.back().enabled);
    CHECK(events.back().x == 0.0f && events.back().y == 0.0f
        && events.back().z == 0.0f && events.back().radius == 0.0f);
    RakNet::BitStream arbitrary;
    arbitrary.Write(static_cast<unsigned char>(0xEF));
    arbitrary.Write(true);
    arbitrary.SetReadOffset(4);
    const auto before = Snapshot(arbitrary);
    CHECK(adapter.ProcessRpcForTesting(kDisableCheckpoint, &arbitrary));
    CHECK(Unchanged(arbitrary, before));
    CHECK(events.size() == 2 && !events.back().enabled);
}

void TestLifecycleRequiresInitializationCallbackAndSupportsSafeReinitialize() {
    keybinder::ServerCheckpointRPCAdapter adapter;
    RakNet::BitStream payload;
    WriteCheckpoint(payload);
    const auto before = Snapshot(payload);
    int delivered = 0;
    CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
    CHECK(Unchanged(payload, before));
    adapter.SetCallback([&](const auto&) { ++delivered; });
    CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
    CHECK(!adapter.ProcessRpcForTesting(kDisableCheckpoint, nullptr));
    CHECK(adapter.Initialize());
    CHECK(adapter.Initialize());
    CHECK(adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
    CHECK(delivered == 1 && Unchanged(payload, before));
    adapter.SetCallback([&](const auto&) { delivered += 10; });
    CHECK(adapter.ProcessRpcForTesting(kDisableCheckpoint, nullptr));
    CHECK(delivered == 11);
    adapter.Shutdown();
    adapter.Shutdown();
    CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
    CHECK(!adapter.ProcessRpcForTesting(kDisableCheckpoint, nullptr));
    CHECK(delivered == 11 && Unchanged(payload, before));
    CHECK(adapter.Initialize());
    CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
    CHECK(!adapter.ProcessRpcForTesting(kDisableCheckpoint, nullptr));
    adapter.SetCallback([&](const auto&) { delivered += 100; });
    CHECK(adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
    CHECK(delivered == 111 && Unchanged(payload, before));
    adapter.SetCallback({});
    CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
    CHECK(!adapter.ProcessRpcForTesting(kDisableCheckpoint, nullptr));
    CHECK(Unchanged(payload, before));
    {
        keybinder::ServerCheckpointRPCAdapter scoped;
        scoped.SetCallback([&](const auto&) { delivered += 1000; });
        CHECK(scoped.Initialize());
        CHECK(scoped.ProcessRpcForTesting(kDisableCheckpoint, nullptr));
        scoped.Shutdown();
        CHECK(!scoped.ProcessRpcForTesting(kDisableCheckpoint, nullptr));
    }
    CHECK(delivered == 1111);
}

void TestCallbackFailureStillLeavesTheIncomingPayloadReadOnly() {
    keybinder::ServerCheckpointRPCAdapter adapter;
    adapter.SetCallback([](const auto&) { throw std::runtime_error("test callback failure"); });
    CHECK(adapter.Initialize());
    RakNet::BitStream payload;
    payload.Write(true);
    WriteCheckpoint(payload);
    payload.SetReadOffset(1);
    const auto before = Snapshot(payload);
    bool threw = false;
    try { adapter.ProcessRpcForTesting(kSetCheckpoint, &payload); }
    catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    CHECK(Unchanged(payload, before));
    adapter.Shutdown();
    CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
    CHECK(Unchanged(payload, before));
}

void TestConcurrentShutdownAndDispatchDoNotDeliverAfterShutdownReturns() {
    keybinder::ServerCheckpointRPCAdapter adapter;
    std::atomic<int> callbacks{0};
    std::atomic<bool> malformed{false};
    std::atomic<bool> start{false};
    auto callback = [&](const keybinder::ServerCheckpointEvent& event) {
        if (event.enabled && (event.x != 1.25f || event.y != -2.5f
            || event.z != 3.75f || event.radius != 4.0f)) malformed = true;
        ++callbacks;
    };
    adapter.SetCallback(callback);
    CHECK(adapter.Initialize());
    std::thread dispatcher([&]() {
        RakNet::BitStream payload;
        payload.Write(true); payload.Write(false);
        WriteCheckpoint(payload, 1.25f, -2.5f, 3.75f, 4.0f);
        payload.SetReadOffset(2);
        const auto before = Snapshot(payload);
        while (!start.load()) std::this_thread::yield();
        for (int i = 0; i < 1000; ++i) {
            adapter.ProcessRpcForTesting(kSetCheckpoint, &payload);
            adapter.ProcessRpcForTesting(kDisableCheckpoint, nullptr);
            if (adapter.ProcessRpcForTesting(38, &payload) || !Unchanged(payload, before)) malformed = true;
        }
    });
    std::thread lifecycle([&]() {
        while (!start.load()) std::this_thread::yield();
        for (int i = 0; i < 100; ++i) {
            adapter.Shutdown();
            adapter.SetCallback(callback);
            adapter.Initialize();
        }
    });
    start = true;
    dispatcher.join();
    lifecycle.join();
    adapter.Shutdown();
    const int afterShutdown = callbacks.load();
    RakNet::BitStream payload;
    WriteCheckpoint(payload, 1.25f, -2.5f, 3.75f, 4.0f);
    const auto before = Snapshot(payload);
    for (int i = 0; i < 32; ++i) {
        CHECK(!adapter.ProcessRpcForTesting(kSetCheckpoint, &payload));
        CHECK(!adapter.ProcessRpcForTesting(kDisableCheckpoint, nullptr));
        CHECK(Unchanged(payload, before));
    }
    CHECK(callbacks.load() == afterShutdown);
    CHECK(!malformed.load());
}
} // namespace

int RunServerCheckpointRPCAdapterTests() {
    checks = failures = 0;
    TestSetCheckpointParsesEveryBitAlignmentAndDoesNotMutateTheRpc();
    TestEveryTruncatedPayloadRestoresTheOriginalReadPositionAndBytes();
    TestNanInfinityInvalidCoordinatesAndRadiusNeverReachTheCallback();
    TestCoordinateAndRadiusBoundariesRemainValidAndReadOnly();
    TestUnrelatedRpcIdsAreIgnored();
    TestRacingCheckpointLayoutAndDisablePreservePayload();
    TestDisableCheckpointAllowsNullOrUnrelatedPayloadWithoutReadingIt();
    TestLifecycleRequiresInitializationCallbackAndSupportsSafeReinitialize();
    TestCallbackFailureStillLeavesTheIncomingPayloadReadOnly();
    TestConcurrentShutdownAndDispatchDoNotDeliverAfterShutdownReturns();
    std::printf("ServerCheckpointRPCAdapterTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
