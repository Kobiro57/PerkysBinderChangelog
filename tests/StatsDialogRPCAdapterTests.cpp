#include "../include/StatsDialogRPCAdapter.h"
#include "../include/RakHook/include/RakNet/BitStream.h"
#include "../include/RakHook/include/RakNet/StringCompressor.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] StatsDialogRPC: %s (line %d)\n", #value, __LINE__); return; } } while (0)

void WriteByteString(RakNet::BitStream& payload, const std::string& text) {
    payload.Write(static_cast<unsigned char>(text.size()));
    if (!text.empty()) payload.Write(text.data(), static_cast<int>(text.size()));
}

void WriteDialog(RakNet::BitStream& payload, unsigned char style, const std::string& caption,
                 const std::string& body, unsigned short id = 104) {
    payload.Write(id);
    payload.Write(style);
    WriteByteString(payload, caption);
    WriteByteString(payload, "Schliessen");
    WriteByteString(payload, "");
    StringCompressor::Instance()->EncodeString(body.c_str(), 0, &payload);
}

void TestAllReadOnlyDialogStylesDeliverFullCompressedTextAndPreserveOffset() {
    keybinder::StatsDialogRPCAdapter adapter;
    std::vector<keybinder::StatsDialogEvent> events;
    adapter.SetDialogCallback([&](const auto& event) { events.push_back(event); });
    CHECK(adapter.Initialize());
    const std::string body = "Wert\tStand\nKills\t123\nTode\t45\nBargeld\t1.234$\nKontostand\t5.678$\n" + std::string(6000, 'a');
    for (unsigned char style : {0, 1, 2, 4, 5}) {
        RakNet::BitStream payload;
        payload.Write(true); payload.Write(false); payload.Write(true);
        const int start = payload.GetNumberOfBitsUsed();
        WriteDialog(payload, style, "{FFFFFF}Statistik von Fixture_Player", body);
        payload.SetReadOffset(start);
        CHECK(adapter.ProcessRpcForTesting(61, &payload));
        CHECK(payload.GetReadOffset() == start);
        CHECK(events.back().id == 104 && events.back().style == style);
        CHECK(events.back().caption == "{FFFFFF}Statistik von Fixture_Player");
        CHECK(events.back().text == body);
    }
    CHECK(events.size() == 5);
    adapter.Shutdown();
}

void TestMalformedTruncatedAndInputPayloadsDoNotDeliverOrConsume() {
    keybinder::StatsDialogRPCAdapter adapter;
    int delivered = 0;
    adapter.SetDialogCallback([&](const auto&) { ++delivered; });
    CHECK(adapter.Initialize());
    CHECK(!adapter.ProcessRpcForTesting(61, nullptr));
    RakNet::BitStream complete;
    WriteDialog(complete, 4, "Statistik", "Kills\t12\nTode\t4");
    const int completeBits = complete.GetNumberOfBitsUsed();
    for (int bits = 0; bits < completeBits; ++bits) {
        RakNet::BitStream truncated(complete.GetData(), complete.GetNumberOfBytesUsed(), true);
        truncated.SetWriteOffset(bits);
        CHECK(!adapter.ProcessRpcForTesting(61, &truncated));
        CHECK(truncated.GetReadOffset() == 0);
    }
    for (unsigned char style : {3, 6, 255}) {
        RakNet::BitStream payload;
        WriteDialog(payload, style, "Statistik", "Kills: [12]");
        CHECK(!adapter.ProcessRpcForTesting(61, &payload));
        CHECK(payload.GetReadOffset() == 0);
    }
    CHECK(!adapter.ProcessRpcForTesting(93, &complete));
    CHECK(complete.GetReadOffset() == 0 && delivered == 0);
    RakNet::BitStream closeDialog;
    WriteDialog(closeDialog, 0, "", "", 0xFFFF);
    CHECK(!adapter.ProcessRpcForTesting(61, &closeDialog));
    CHECK(closeDialog.GetReadOffset() == 0);
    RakNet::BitStream embeddedNull;
    WriteDialog(embeddedNull, 0, std::string("Stats\0foreign", 13), "Kills: [12]");
    CHECK(!adapter.ProcessRpcForTesting(61, &embeddedNull));
    CHECK(delivered == 0);
}

void TestLifecycleNeverDeliversAfterShutdownAndSupportsReinitialize() {
    keybinder::StatsDialogRPCAdapter adapter;
    int delivered = 0;
    adapter.SetDialogCallback([&](const auto&) { ++delivered; });
    RakNet::BitStream noPayload;
    CHECK(!adapter.ProcessRpcForTesting(61, &noPayload));
    CHECK(adapter.Initialize());
    CHECK(adapter.Initialize());
    RakNet::BitStream payload;
    WriteDialog(payload, 0, "Stats", "Kills: [12]");
    CHECK(adapter.ProcessRpcForTesting(61, &payload));
    CHECK(delivered == 1);
    adapter.Shutdown();
    CHECK(!adapter.ProcessRpcForTesting(61, &payload));
    CHECK(delivered == 1 && payload.GetReadOffset() == 0);
    adapter.Shutdown();
    adapter.SetDialogCallback([&](const auto&) { delivered += 10; });
    CHECK(adapter.Initialize());
    CHECK(adapter.ProcessRpcForTesting(61, &payload));
    CHECK(delivered == 11);
    adapter.Shutdown();
}
} // namespace

int RunStatsDialogRPCAdapterTests() {
    checks = failures = 0;
    TestAllReadOnlyDialogStylesDeliverFullCompressedTextAndPreserveOffset();
    TestMalformedTruncatedAndInputPayloadsDoNotDeliverOrConsume();
    TestLifecycleNeverDeliversAfterShutdownAndSupportsReinitialize();
    std::printf("StatsDialogRPCAdapterTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
