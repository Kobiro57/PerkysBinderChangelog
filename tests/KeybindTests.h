#pragma once

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include "../include/Action.h"
#include "../include/Keybind.h"
#include "../include/KeybindManager.h"
#include "../include/Logger.h"

using namespace keybinder;

struct TestResult {
    int passed = 0;
    int failed = 0;
    std::string last_test;
    std::string last_error;
};

static TestResult g_results;

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        g_results.failed++; \
        g_results.last_error = "Assertion failed: " #cond; \
        return false; \
    } \
    g_results.passed++; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        g_results.failed++; \
        g_results.last_error = "Equality check failed: " #a " != " #b; \
        return false; \
    } \
    g_results.passed++; \
} while(0)

#define ASSERT_NE(a, b) do { \
    if ((a) == (b)) { \
        g_results.failed++; \
        g_results.last_error = "Inequality check failed: " #a " == " #b; \
        return false; \
    } \
    g_results.passed++; \
} while(0)

#define RUN_TEST(name) do { \
    g_results.last_test = #name; \
    if (name()) { \
        printf("[PASS] %s\n", #name); \
    } else { \
        printf("[FAIL] %s: %s\n", #name, g_results.last_error.c_str()); \
    } \
} while(0)

inline bool TestActionEquality() {
    Action a1(ActionType::Chat, "Hello");
    Action a2(ActionType::Chat, "Hello");
    Action a3(ActionType::Command, "/me waves");
    
    ASSERT_EQ(a1 == a2, true);
    ASSERT_EQ(a1 == a3, false);
    ASSERT_EQ(a1 != a3, true);
    return true;
}

inline bool TestKeybindEquality() {
    Keybind kb1;
    kb1.name = "Test";
    kb1.key = 0x46;
    kb1.category = "Test";
    kb1.enabled = true;
    kb1.passthrough = false;
    kb1.actions.push_back(Action(ActionType::Chat, "Test message"));
    
    Keybind kb2;
    kb2.name = "Test";
    kb2.key = 0x46;
    kb2.category = "Test";
    kb2.enabled = true;
    kb2.passthrough = false;
    kb2.actions.push_back(Action(ActionType::Chat, "Test message"));
    
    ASSERT_EQ(kb1 == kb2, true);
    
    kb2.key = 0x47;
    ASSERT_EQ(kb1 == kb2, false);
    return true;
}

inline bool TestKeybindManagerAddGet() {
    keybinder::KeybindManager mgr("test_keybinds.json");
    
    Keybind kb1;
    kb1.name = "Bind1";
    kb1.key = 0x46;
    kb1.actions.push_back(Action(ActionType::Chat, "Hello"));
    
    Keybind kb2;
    kb2.name = "Bind2";
    kb2.key = 0x47;
    kb2.actions.push_back(Action(ActionType::Command, "/me waves"));
    
    mgr.Add(kb1);
    mgr.Add(kb2);
    
    ASSERT_EQ(mgr.Count(), 2);
    
    const Keybind* retrieved = mgr.Get(0);
    ASSERT_TRUE(retrieved != nullptr);
    ASSERT_EQ(retrieved->name, "Bind1");
    
    retrieved = mgr.Get(1);
    ASSERT_TRUE(retrieved != nullptr);
    ASSERT_EQ(retrieved->name, "Bind2");
    
    retrieved = mgr.Get(5);
    ASSERT_TRUE(retrieved == nullptr);
    
    return true;
}

inline bool TestKeybindManagerRemove() {
    keybinder::KeybindManager mgr("test_keybinds.json");
    
    mgr.Add(Keybind("Bind1", 0x46));
    mgr.Add(Keybind("Bind2", 0x47));
    mgr.Add(Keybind("Bind3", 0x48));
    
    ASSERT_EQ(mgr.Count(), 3);
    
    ASSERT_TRUE(mgr.Remove(1));
    ASSERT_EQ(mgr.Count(), 2);
    
    const Keybind* retrieved = mgr.Get(1);
    ASSERT_TRUE(retrieved != nullptr);
    ASSERT_EQ(retrieved->name, "Bind3");
    
    ASSERT_TRUE(mgr.Remove(0));
    ASSERT_TRUE(mgr.Remove(0));
    ASSERT_EQ(mgr.Count(), 0);
    
    ASSERT_TRUE(mgr.Remove(10) == false);
    
    return true;
}

inline bool TestKeybindManagerGetByName() {
    keybinder::KeybindManager mgr("test_keybinds.json");
    
    mgr.Add(Keybind("Alpha", 0x41));
    mgr.Add(Keybind("Beta", 0x42));
    mgr.Add(Keybind("Gamma", 0x47));
    
    const Keybind* found = mgr.GetByName("Beta");
    ASSERT_TRUE(found != nullptr);
    ASSERT_EQ(found->key, 0x42);
    
    found = mgr.GetByName("Delta");
    ASSERT_TRUE(found == nullptr);
    
    return true;
}

inline bool TestKeybindManagerGetByKey() {
    keybinder::KeybindManager mgr("test_keybinds.json");
    
    Keybind kb1;
    kb1.name = "Bind1";
    kb1.key = 0x46;
    
    Keybind kb2;
    kb2.name = "Bind2";
    kb2.key = 0x47;
    
    Keybind kb3;
    kb3.name = "Bind3";
    kb3.key = 0x48;
    
    kb1.actions.push_back(Action(ActionType::Chat, "one"));
    kb2.actions.push_back(Action(ActionType::Chat, "two"));
    kb3.actions.push_back(Action(ActionType::Chat, "three"));
    
    mgr.Add(kb1);
    mgr.Add(kb2);
    mgr.Add(kb3);
    
    auto indices = mgr.GetByKey(0x46);
    ASSERT_EQ(indices.size(), 1);
    ASSERT_EQ(mgr.Get(indices[0])->name, "Bind1");
    
    indices = mgr.GetByKey(0x47);
    ASSERT_EQ(indices.size(), 1);
    ASSERT_EQ(mgr.Get(indices[0])->name, "Bind2");
    
    indices = mgr.GetByKey(0x48);
    ASSERT_EQ(indices.size(), 1);
    ASSERT_EQ(mgr.Get(indices[0])->name, "Bind3");
    
    indices = mgr.GetByKey(0x49);
    ASSERT_EQ(indices.size(), 0);
    
    return true;
}

inline bool TestSerializationRoundTrip() {
    keybinder::KeybindManager mgr("test_roundtrip.json");
    
    Keybind kb1;
    kb1.name = "Test Bind with {zone}";
    kb1.key = 0x46;
    kb1.category = "Communication";
    kb1.enabled = true;
    kb1.passthrough = false;
    kb1.actions.push_back(Action(ActionType::Chat, "Hello from {zone}!"));
    kb1.actions.push_back(Action(ActionType::Delay, "500"));
    kb1.actions.push_back(Action(ActionType::Command, "/me waves"));
    kb1.actions.push_back(Action(ActionType::KeyPress, "F1"));
    
    Keybind kb2;
    kb2.name = "Another Bind";
    kb2.key = 0x23;
    kb2.category = "Movement";
    kb2.enabled = false;
    kb2.passthrough = true;
    kb2.actions.push_back(Action(ActionType::Chat, "Moving to safe zone..."));
    
    mgr.Add(kb1);
    mgr.Add(kb2);
    
    ASSERT_TRUE(mgr.Save());
    
    keybinder::KeybindManager mgr2("test_roundtrip.json");
    ASSERT_TRUE(mgr2.Load());
    ASSERT_EQ(mgr2.Count(), 2);
    
    const Keybind* loaded = mgr2.Get(0);
    ASSERT_TRUE(loaded != nullptr);
    ASSERT_EQ(loaded->name, "Test Bind with {zone}");
    ASSERT_EQ(loaded->key, 0x46);
    ASSERT_EQ(loaded->category, "Communication");
    ASSERT_EQ(loaded->enabled, true);
    ASSERT_EQ(loaded->passthrough, false);
    ASSERT_EQ(loaded->actions.size(), 4);
    ASSERT_EQ(loaded->actions[0].type, ActionType::Chat);
    ASSERT_EQ(loaded->actions[0].data, "Hello from {zone}!");
    ASSERT_EQ(loaded->actions[1].type, ActionType::Delay);
    ASSERT_EQ(loaded->actions[1].data, "500");
    ASSERT_EQ(loaded->actions[2].type, ActionType::Command);
    ASSERT_EQ(loaded->actions[2].data, "/me waves");
    ASSERT_EQ(loaded->actions[3].type, ActionType::KeyPress);
    ASSERT_EQ(loaded->actions[3].data, "F1");
    
    loaded = mgr2.Get(1);
    ASSERT_TRUE(loaded != nullptr);
    ASSERT_EQ(loaded->name, "Another Bind");
    ASSERT_EQ(loaded->key, 0x23);
    ASSERT_EQ(loaded->category, "Movement");
    ASSERT_EQ(loaded->enabled, false);
    ASSERT_EQ(loaded->passthrough, true);
    
    std::remove("test_roundtrip.json");
    
    return true;
}

inline bool TestCorruptConfigHandling() {
    {
        FILE* f = std::fopen("test_corrupt.json", "w");
        if (f) {
            std::fputs("{ invalid json content !!!", f);
            std::fclose(f);
        }
    }
    
    keybinder::KeybindManager mgr("test_corrupt.json");
    bool result = mgr.Load();
    
    ASSERT_TRUE(result == false);
    ASSERT_EQ(mgr.Count(), 0);
    
    {
        FILE* f = std::fopen("test_corrupt.json", "w");
        if (f) {
            std::fputs("{\"version\": 1, \"keybinds\": [", f);
            std::fclose(f);
        }
    }
    
    result = mgr.Load();
    ASSERT_TRUE(result == false);
    ASSERT_EQ(mgr.Count(), 0);
    
    {
        FILE* f = std::fopen("test_corrupt.json", "w");
        if (f) {
            std::fputs("{\"version\": 1, \"keybinds\": \"not an array\"}", f);
            std::fclose(f);
        }
    }
    
    result = mgr.Load();
    ASSERT_TRUE(result == false);
    ASSERT_EQ(mgr.Count(), 0);
    
    {
        FILE* f = std::fopen("test_corrupt.json", "w");
        if (f) {
            std::fputs("{\"version\": 1, \"keybinds\": [{\"name\": \"Test\", \"key\": 0x46, \"ctrl\": true, \"shift\": false, \"alt\": false, \"category\": \"Test\", \"enabled\": true, \"passthrough\": false, \"actions\": \"not an array\"}]}", f);
            std::fclose(f);
        }
    }
    
    result = mgr.Load();
    ASSERT_TRUE(result == false);
    ASSERT_EQ(mgr.Count(), 0);
    
    std::remove("test_corrupt.json");
    
    return true;
}

inline bool TestEmptyConfigHandling() {
    keybinder::KeybindManager mgr("test_empty.json");
    
    bool result = mgr.Load();
    ASSERT_TRUE(result == true);
    ASSERT_EQ(mgr.Count(), 0);
    
    result = mgr.Save();
    ASSERT_TRUE(result == true);
    
    result = mgr.Load();
    ASSERT_TRUE(result == true);
    ASSERT_EQ(mgr.Count(), 0);
    
    std::remove("test_empty.json");
    
    return true;
}

inline bool TestAllActionTypes() {
    keybinder::KeybindManager mgr("test_actions.json");
    
    Keybind kb;
    kb.name = "All Types";
    kb.key = 0x50;
    kb.actions.push_back(Action(ActionType::Chat, "Chat message"));
    kb.actions.push_back(Action(ActionType::Command, "/command arg"));
    kb.actions.push_back(Action(ActionType::Delay, "1000"));
    kb.actions.push_back(Action(ActionType::KeyPress, "Enter"));
    
    mgr.Add(kb);
    ASSERT_TRUE(mgr.Save());
    
    keybinder::KeybindManager mgr2("test_actions.json");
    ASSERT_TRUE(mgr2.Load());
    ASSERT_EQ(mgr2.Count(), 1);
    
    const Keybind* loaded = mgr2.Get(0);
    ASSERT_TRUE(loaded != nullptr);
    ASSERT_EQ(loaded->actions.size(), 4);
    ASSERT_EQ(loaded->actions[0].type, ActionType::Chat);
    ASSERT_EQ(loaded->actions[1].type, ActionType::Command);
    ASSERT_EQ(loaded->actions[2].type, ActionType::Delay);
    ASSERT_EQ(loaded->actions[3].type, ActionType::KeyPress);
    
    std::remove("test_actions.json");
    
    return true;
}

inline bool TestKeybindManagerGetBySlashCommand() {
    keybinder::KeybindManager mgr("test_slash.json");
    
    Keybind kb1;
    kb1.name = "Heal Command";
    kb1.slashCommand = "heal";
    kb1.key = 0;
    kb1.actions.push_back(Action(ActionType::Command, "/healme"));
    
    Keybind kb2;
    kb2.name = "Dual Trigger";
    kb2.slashCommand = "test";
    kb2.key = 0x48;
    kb2.actions.push_back(Action(ActionType::Chat, "Test message"));
    
    Keybind kb3;
    kb3.name = "No Slash";
    kb3.key = 0x49;
    kb3.actions.push_back(Action(ActionType::Chat, "No slash command"));
    
    mgr.Add(kb1);
    mgr.Add(kb2);
    mgr.Add(kb3);
    
    auto indices = mgr.GetBySlashCommand("heal");
    ASSERT_EQ(indices.size(), 1);
    ASSERT_EQ(mgr.Get(indices[0])->name, "Heal Command");
    
    indices = mgr.GetBySlashCommand("/heal");
    ASSERT_EQ(indices.size(), 1);
    
    indices = mgr.GetBySlashCommand("HEAL");
    ASSERT_EQ(indices.size(), 1);
    
    indices = mgr.GetBySlashCommand("test");
    ASSERT_EQ(indices.size(), 1);
    ASSERT_EQ(mgr.Get(indices[0])->name, "Dual Trigger");

    // Exact match only - prefixes must not trigger other commands
    indices = mgr.GetBySlashCommand("hea");
    ASSERT_EQ(indices.size(), 0);
    indices = mgr.GetBySlashCommand("healer");
    ASSERT_EQ(indices.size(), 0);

    indices = mgr.GetBySlashCommand("unknown");
    ASSERT_EQ(indices.size(), 0);
    
    indices = mgr.GetBySlashCommand("");
    ASSERT_EQ(indices.size(), 0);
    
    std::remove("test_slash.json");
    
    return true;
}

inline bool TestSlashCommandSerialization() {
    keybinder::KeybindManager mgr("test_slash_serialize.json");
    
    Keybind kb;
    kb.name = "Slash Test";
    kb.key = 0x46;
    kb.slashCommand = "mycommand";
    kb.actions.push_back(Action(ActionType::Chat, "Hello"));
    
    mgr.Add(kb);
    ASSERT_TRUE(mgr.Save());
    
    keybinder::KeybindManager mgr2("test_slash_serialize.json");
    ASSERT_TRUE(mgr2.Load());
    ASSERT_EQ(mgr2.Count(), 1);
    
    const Keybind* loaded = mgr2.Get(0);
    ASSERT_TRUE(loaded != nullptr);
    ASSERT_EQ(loaded->slashCommand, "mycommand");
    
    auto indices = mgr2.GetBySlashCommand("mycommand");
    ASSERT_EQ(indices.size(), 1);
    
    std::remove("test_slash_serialize.json");
    
    return true;
}

inline bool TestSlashCommandOnlyKeybind() {
    keybinder::KeybindManager mgr("test_slash_only.json");
    
    Keybind kb;
    kb.name = "Slash Only";
    kb.key = 0;
    kb.slashCommand = "onlyslash";
    kb.actions.push_back(Action(ActionType::Command, "/dosomething"));
    
    mgr.Add(kb);
    
    auto indices = mgr.GetBySlashCommand("onlyslash");
    ASSERT_EQ(indices.size(), 1);
    
    const Keybind* loaded = mgr.Get(indices[0]);
    ASSERT_TRUE(loaded != nullptr);
    ASSERT_EQ(loaded->GetAllKeys().size(), 0);
    ASSERT_EQ(loaded->slashCommand, "onlyslash");
    
    ASSERT_TRUE(mgr.Save());
    
    keybinder::KeybindManager mgr2("test_slash_only.json");
    ASSERT_TRUE(mgr2.Load());
    ASSERT_EQ(mgr2.Count(), 1);
    
    loaded = mgr2.Get(0);
    ASSERT_TRUE(loaded != nullptr);
    ASSERT_EQ(loaded->GetAllKeys().size(), 0);
    ASSERT_EQ(loaded->slashCommand, "onlyslash");
    
    std::remove("test_slash_only.json");
    
    return true;
}

inline int RunKeybindTests() {
    printf("\n=== Keybind Model Tests ===\n\n");
    
    keybinder::Logger::Initialize("test_keybind.log");
    keybinder::Logger::SetMinLevel(keybinder::LogLevel::Debug);
    
    RUN_TEST(TestActionEquality);
    RUN_TEST(TestKeybindEquality);
    RUN_TEST(TestKeybindManagerAddGet);
    RUN_TEST(TestKeybindManagerRemove);
    RUN_TEST(TestKeybindManagerGetByName);
    RUN_TEST(TestKeybindManagerGetByKey);
    RUN_TEST(TestSerializationRoundTrip);
    RUN_TEST(TestCorruptConfigHandling);
    RUN_TEST(TestEmptyConfigHandling);
    RUN_TEST(TestAllActionTypes);
    RUN_TEST(TestKeybindManagerGetBySlashCommand);
    RUN_TEST(TestSlashCommandSerialization);
    RUN_TEST(TestSlashCommandOnlyKeybind);
    
    printf("\n=== Results: %d passed, %d failed ===\n\n", 
           g_results.passed, g_results.failed);
    
    return g_results.failed;
}
