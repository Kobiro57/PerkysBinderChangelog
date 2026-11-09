#include "../include/LocalFactionAccess.h"
#include <cstdio>

namespace {
int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("[FAIL] LocalFactionAccess: %s at %d\n", #x, __LINE__); } } while (0)
}

int RunLocalFactionAccessTests() {
    using namespace keybinder;
    checks = failures = 0;
    CHECK(ReconcileLocalFaction("", {}).empty());
    CHECK(IsRcbMember(ReconcileLocalFaction("Red County Brotherhood", {})));
    CHECK(ReconcileLocalFaction("", std::string("RCB")).empty()); // Same nickname on another server is insufficient.
    CHECK(IsRcbMember(ReconcileLocalFaction("RCB", std::string("Red County Brotherhood"))));
    CHECK(IsPoliceMember(ReconcileLocalFaction("SAPD", std::string("LSPD"))));
    CHECK(ReconcileLocalFaction("", std::string("Los Santos Police Department")).empty());
    CHECK(IsPoliceMember(ReconcileLocalFaction("San Andreas Police Department", {})));
    CHECK(ReconcileLocalFaction("RCB", std::string("Zivilist")).empty());
    CHECK(ReconcileLocalFaction("Zivilist", std::string("RCB")).empty());
    CHECK(ReconcileLocalFaction("SAPD", std::string("RCB")).empty());
    CHECK(ReconcileLocalFaction("RCB", std::string("SAPD")).empty());
    CHECK(!IsRcbMember(ReconcileLocalFaction("Army", std::string("Zivilist"))));
    CHECK(!IsPoliceMember(ReconcileLocalFaction("RCB", {})));
    CHECK(!IsPoliceMember(ReconcileLocalFaction("Not LSPD", {})));
    CHECK(!IsRcbMember(ReconcileLocalFaction("Red County Brotherhood Supporter", {})));
    CHECK(!IsRcbMember(ReconcileLocalFaction("Zivilist", {})));
    CHECK(IsRcbMember(ReconcileLocalFaction(" {FFFFFF}red county brotherhood ", {})));
    CHECK(!IsPoliceMember(ReconcileLocalFaction("", {})));
    std::printf("LocalFactionAccessTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
