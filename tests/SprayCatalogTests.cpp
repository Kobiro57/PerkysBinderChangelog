#include "../include/SprayCatalog.h"
#include "../include/SpraySpots.h"
#include "../include/pugixml/pugixml.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

namespace {
int checks = 0, failures = 0;
#define CHECK(value) do { ++checks; if (!(value)) { ++failures; std::printf("[FAIL] SprayCatalog: %s (line %d)\n", #value, __LINE__); } } while (0)

std::string Utf8Path(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

struct Fixture {
    std::filesystem::path directory, file;
    keybinder::SprayCatalog catalog;
    Fixture() {
        static unsigned int nextFixture = 0;
        directory = std::filesystem::path("build/spray-catalog-tests") /
            (std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()) + "-" + std::to_string(++nextFixture));
        std::filesystem::create_directories(directory);
        file = directory / "sprays.xml";
    }
    bool Initialize() { return catalog.Initialize(Utf8Path(file)); }
    std::string Xml() const {
        std::ifstream input(file, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }
    void Write(const std::string& text) {
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        output.close();
        CHECK(output.good());
    }
    void Seed(std::size_t count) {
        pugi::xml_document document;
        auto root = document.append_child("nlbinder-sprays");
        root.append_attribute("version") = 1;
        root.append_attribute("next-id") = static_cast<unsigned int>((std::max)(std::size_t(101), count + 1));
        for (unsigned int index = 1; index <= count; ++index) {
            auto point = root.append_child("point");
            point.append_attribute("id") = index;
            point.append_attribute("name") = ("Testpunkt " + std::to_string(index)).c_str();
            point.append_attribute("x") = 100.0f + index;
            point.append_attribute("y") = 200.0f;
            point.append_attribute("z") = 30.0f;
            point.append_attribute("interior") = 0;
        }
        CHECK(document.save_file(file.c_str(), "  ", pugi::format_default, pugi::encoding_utf8));
    }
};

void DefaultsAreOnlyTheStartingListAndChangesSurviveReload() {
    Fixture fixture;
    CHECK(fixture.Initialize());
    CHECK(fixture.catalog.IsWritable());
    CHECK(fixture.catalog.LastError().empty());
    CHECK(fixture.catalog.Entries().size() == 100);
    CHECK(!std::filesystem::exists(fixture.file)); // No automatic replacement/write.
    for (const auto& point : keybinder::kSpraySpots) {
        const auto* found = fixture.catalog.Find(point.id);
        CHECK(found && found->name == point.place);
        CHECK(found && found->position.x == point.position.x && found->position.y == point.position.y
            && found->position.z == point.position.z && found->position.interior == 0);
    }
    CHECK(fixture.catalog.Remove(14));
    CHECK(!fixture.catalog.Find(14));
    CHECK(fixture.catalog.Entries().size() == 99);
    CHECK(fixture.catalog.Add("  Eigener Punkt  ", {123.125f, -456.75f, 32.0625f, 0}));
    const auto* added = fixture.catalog.Find(101);
    CHECK(added && added->name == "Eigener Punkt");
    CHECK(added && added->position.x == 123.125f && added->position.y == -456.75f && added->position.z == 32.0625f);
    const auto before = fixture.Xml();
    keybinder::SprayCatalog loaded;
    CHECK(loaded.Initialize(Utf8Path(fixture.file)));
    CHECK(loaded.Entries().size() == 100 && !loaded.Find(14) && loaded.Find(101));
    CHECK(loaded.Find(101)->position.x == 123.125f && loaded.Find(101)->position.z == 32.0625f);
    CHECK(fixture.Xml() == before);
    CHECK(loaded.Remove(101));
    CHECK(loaded.Add("Anderer Punkt", {5, 6, 7, 0}));
    CHECK(loaded.Find(102) && !loaded.Find(101)); // Deleted numbers are never recycled.
}

void EmptyCatalogAndTenPageLimitArePersistent() {
    Fixture empty;
    empty.Seed(0);
    CHECK(empty.Initialize());
    CHECK(empty.catalog.IsWritable() && empty.catalog.Entries().empty());
    CHECK(empty.catalog.Add("Ein Punkt", {10, 20, 30, 0}));
    CHECK(empty.catalog.Remove(101));
    CHECK(empty.Initialize());
    CHECK(empty.catalog.Entries().empty()); // Deleted defaults must not reappear.

    Fixture capacity;
    capacity.Seed(339);
    CHECK(capacity.Initialize());
    CHECK(capacity.catalog.Add("Seite 10", {10, 20, 30, 0}));
    CHECK(capacity.catalog.Entries().size() == keybinder::SprayCatalog::kMaximumEntries);
    CHECK(capacity.catalog.Find(340));
    const auto full = capacity.Xml();
    CHECK(!capacity.catalog.Add("Seite 11", {10, 20, 30, 0}));
    CHECK(capacity.catalog.LastError().find("340") != std::string::npos);
    CHECK(capacity.Xml() == full && capacity.catalog.Entries().size() == 340);
    CHECK(capacity.catalog.Remove(17));
    CHECK(capacity.catalog.Add("Ersatz", {10, 20, 30, 0}));
    CHECK(capacity.catalog.Find(341) && !capacity.catalog.Find(17));
    CHECK(capacity.Initialize());
    CHECK(capacity.catalog.Entries().size() == 340 && capacity.catalog.Find(341) && !capacity.catalog.Find(17));
}

void InvalidInputNeverChangesMemoryOrDisk() {
    Fixture fixture;
    CHECK(fixture.Initialize());
    CHECK(fixture.catalog.Add("Basis", {10, 20, 30, 0}));
    const auto before = fixture.Xml();
    for (const auto& name : std::vector<std::string>{"", "  ", "Bad\nName", "\tBad", std::string("A\0B", 3),
        std::string(65, 'A'), std::string("\xC3", 1), std::string("\xFF", 1)}) {
        CHECK(!fixture.catalog.Add(name, {10, 20, 30, 0}));
        CHECK(!fixture.catalog.LastError().empty());
    }
    for (auto position : {keybinder::GpsPosition{std::numeric_limits<float>::quiet_NaN(), 1, 2, 0},
        keybinder::GpsPosition{1, std::numeric_limits<float>::infinity(), 2, 0},
        keybinder::GpsPosition{1, 2, std::numeric_limits<float>::infinity(), 0},
        keybinder::GpsPosition{1000001, 1, 2, 0}, keybinder::GpsPosition{1, 2, 3, 1}, keybinder::GpsPosition{1, 2, 3, -1}})
        CHECK(!fixture.catalog.Add("Bad position", position));
    CHECK(!fixture.catalog.Remove(0));
    CHECK(!fixture.catalog.Remove(9000));
    CHECK(fixture.catalog.Entries().size() == 101 && fixture.catalog.Find(101) && !fixture.catalog.Find(102));
    CHECK(fixture.Xml() == before);
    CHECK(fixture.catalog.Add("  " + std::string(64, 'A') + "  ", {10, 20, 30, 0}));
    CHECK(fixture.catalog.Find(102)->name == std::string(64, 'A'));
    CHECK(fixture.catalog.LastError().empty());
}

void Utf8NamesPathsAndXmlEscapingRoundTrip() {
    Fixture fixture;
    const auto unicodeDirectory = fixture.directory / std::filesystem::path(L"Spray-\u00dcberblick");
    std::filesystem::create_directories(unicodeDirectory);
    fixture.file = unicodeDirectory / L"Spr\u00e4ys.xml";
    CHECK(fixture.Initialize());
    const std::string name = "B\xC3\xBC" "ro <A> & \"Stra\xC3\x9F" "e\"";
    CHECK(fixture.catalog.Add(name, {12.5f, -8.25f, 2.5f, 0}));
    CHECK(fixture.Xml().find("&lt;A") != std::string::npos && fixture.Xml().find("&amp;") != std::string::npos);
    CHECK(fixture.Initialize());
    CHECK(fixture.catalog.Find(101) && fixture.catalog.Find(101)->name == name);
    const std::string emoji = "\xF0\x9F\x98\x80";
    std::string maximum;
    for (int index = 0; index < 32; ++index) maximum += emoji;
    CHECK(fixture.catalog.Add(maximum, {1, 2, 3, 0})); // Same 64 UTF-16-unit limit as GPS.
    CHECK(!fixture.catalog.Add(maximum + emoji, {1, 2, 3, 0}));
    CHECK(fixture.Initialize());
    CHECK(fixture.catalog.Find(102) && fixture.catalog.Find(102)->name == maximum);
}

void CorruptFilesRemainUnmodifiedAndCannotBeOverwrittenByEdits() {
    const std::string point = "<point id=\"1\" name=\"A\" x=\"1\" y=\"2\" z=\"3\" interior=\"0\"/>";
    std::vector<std::string> invalid{
        "", "<broken", "<other/>", "<nlbinder-sprays version=\"2\" next-id=\"101\"/>",
        "<nlbinder-sprays version=\"1\"/>", "<nlbinder-sprays version=\"1\" next-id=\"1\"/>",
        "<nlbinder-sprays version=\"1\" next-id=\"101\">" + point + point + "</nlbinder-sprays>",
        "<nlbinder-sprays version=\"1\" next-id=\"101\">junk</nlbinder-sprays>",
        "<nlbinder-sprays version=\"1\" next-id=\"101\"><unknown/></nlbinder-sprays>",
        "<nlbinder-sprays version=\"1\" next-id=\"101\"><point id=\"101\" name=\"A\" x=\"1\" y=\"2\" z=\"3\" interior=\"0\"/></nlbinder-sprays>",
        "<nlbinder-sprays version=\"1\" next-id=\"101\"/><other/>", std::string(256 * 1024 + 1, 'x')};
    for (const auto& change : std::vector<std::pair<std::string, std::string>>{
        {"id=\"1\"", "id=\"0\""}, {"id=\"1\"", "id=\"1x\""}, {"id=\"1\"", "id=\"2147483648\""},
        {"name=\"A\"", "name=\"\""}, {"x=\"1\"", "x=\"nan\""}, {"x=\"1\"", "x=\"1x\""},
        {"y=\"2\"", "y=\"inf\""}, {"z=\"3\"", "z=\"1000001\""}, {"interior=\"0\"", "interior=\"1\""}}) {
        auto changed = point;
        changed.replace(changed.find(change.first), change.first.size(), change.second);
        invalid.push_back("<nlbinder-sprays version=\"1\" next-id=\"101\">" + changed + "</nlbinder-sprays>");
    }
    for (const auto& text : invalid) {
        Fixture fixture;
        fixture.Write(text);
        CHECK(!fixture.Initialize());
        CHECK(!fixture.catalog.IsWritable() && !fixture.catalog.LastError().empty());
        CHECK(!fixture.catalog.Add("Do not overwrite", {1, 2, 3, 0}));
        CHECK(!fixture.catalog.Remove(1));
        CHECK(fixture.Xml() == text);
    }
    Fixture oversized;
    oversized.Seed(341);
    const auto before = oversized.Xml();
    CHECK(!oversized.Initialize());
    CHECK(!oversized.catalog.IsWritable() && !oversized.catalog.Add("Over limit", {1, 2, 3, 0}));
    CHECK(oversized.Xml() == before);
}

void FailedWritesPreservePointsFileAndNextId() {
    Fixture missing;
    missing.file = missing.directory / "missing-parent" / "sprays.xml";
    CHECK(missing.Initialize());
    CHECK(!missing.catalog.Add("No parent", {1, 2, 3, 0}));
    CHECK(missing.catalog.Entries().size() == 100 && !missing.catalog.Find(101));
    CHECK(!missing.catalog.Remove(14) && missing.catalog.Find(14));
    CHECK(!std::filesystem::exists(missing.file));

    Fixture locked;
    CHECK(locked.Initialize());
    CHECK(locked.catalog.Add("First", {1, 2, 3, 0}));
    const auto before = locked.Xml();
    HANDLE handle = CreateFileW(locked.file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(handle != INVALID_HANDLE_VALUE);
    CHECK(!locked.catalog.Add("Blocked", {1, 2, 3, 0}));
    CHECK(!locked.catalog.Remove(14));
    CHECK(locked.catalog.Find(14) && locked.catalog.Entries().size() == 101 && !locked.catalog.Find(102));
    CHECK(locked.Xml() == before);
    if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    CHECK(locked.catalog.Add("Retry", {1, 2, 3, 0}));
    CHECK(locked.catalog.Find(102) && !locked.catalog.Find(103));
    unsigned int temporaryFiles = 0;
    for (const auto& file : std::filesystem::directory_iterator(locked.directory))
        if (file.path().filename().wstring().find(L".tmp-") != std::wstring::npos) ++temporaryFiles;
    CHECK(temporaryFiles == 0);
}
} // namespace

int RunSprayCatalogTests() {
    checks = failures = 0;
    DefaultsAreOnlyTheStartingListAndChangesSurviveReload();
    EmptyCatalogAndTenPageLimitArePersistent();
    InvalidInputNeverChangesMemoryOrDisk();
    Utf8NamesPathsAndXmlEscapingRoundTrip();
    CorruptFilesRemainUnmodifiedAndCannotBeOverwrittenByEdits();
    FailedWritesPreservePointsFileAndNextId();
    std::printf("SprayCatalogTests: %d checks, %d failures\n", checks, failures);
    return failures;
}
