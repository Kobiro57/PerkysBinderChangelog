#include "../include/KillMessageTest.h"

#include <cstdio>
#include <string>

namespace {
int failures = 0;
int checks = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] Testkill: %s (line %d)\n", #value, __LINE__); } } while (0)

void CheckMessage(const std::string& input, const std::string& expected) {
    std::string message, error;
    CHECK(keybinder::BuildSimulatedKillMessage(input, message, error));
    CHECK(message == expected);
    CHECK(error.empty());
}

void TestChatAndArbitraryCommandChannels() {
    CheckMessage("Kill in Los Santos, Market. Meine Kills: 64", "[Testkill] Kill in Los Santos, Market. Meine Kills: 64");
    CheckMessage("/f Kill in Market", "/f [Testkill] Kill in Market");
    CheckMessage("/d Kill in Market", "/d [Testkill] Kill in Market");
    CheckMessage("/r Kill in Market", "/r [Testkill] Kill in Market");
    CheckMessage("/me meldet einen Test", "/me [Testkill] meldet einen Test");
    CheckMessage("  /custom_channel\t  Nachricht  ", "/custom_channel [Testkill] Nachricht");
    CheckMessage("/f", "/f [Testkill]");
    CheckMessage("Eine Zahl 1/2", "[Testkill] Eine Zahl 1/2");
}

void TestInvalidAndLongMessages() {
    for (const std::string input : {std::string(), std::string(" \t\n"), std::string("/"), std::string("/  "),
                                  std::string("eins\nzwei"), std::string("/f eins\rzwei"), std::string("eins\0zwei", 9)}) {
        std::string message = "old", error;
        CHECK(!keybinder::BuildSimulatedKillMessage(input, message, error));
        CHECK(message.empty());
        CHECK(!error.empty());
    }
    std::string message, error;
    CHECK(keybinder::BuildSimulatedKillMessage("/f " + std::string(200, 'x'), message, error));
    CHECK(message.size() == 144);
    CHECK(message.starts_with("/f [Testkill] "));
    CHECK(keybinder::BuildSimulatedKillMessage(std::string(132, 'a') + "\xC3\xA4", message, error));
    CHECK(message == "[Testkill] " + std::string(132, 'a'));
    CHECK(message.size() == 143);
    CHECK(!keybinder::BuildSimulatedKillMessage("/" + std::string(140, 'x'), message, error));
    CHECK(message.empty());
}
} // namespace

int RunKillMessageTestTests() {
    failures = checks = 0;
    TestChatAndArbitraryCommandChannels();
    TestInvalidAndLongMessages();
    std::printf("KillMessageTestTests: %d checks, %d failures\n", checks, failures);
    return failures;
}

#ifdef KILL_MESSAGE_TEST_MAIN
int main() { return RunKillMessageTestTests(); }
#endif
