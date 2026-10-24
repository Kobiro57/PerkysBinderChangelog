#include "../include/HitsoundPlayer.h"

#include "../include/Logger.h"
#include "../include/PersistentPaths.h"
#include "../include/ResourceIds.h"
#include "../include/SettingsManager.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>

#include <windows.h>
#include <objbase.h>

#ifndef KEYBINDER_TESTING
#include <xaudio2.h>
#endif

namespace {

constexpr int kSelfDamageWeaponId = 54;

std::size_t ToIndex(keybinder::HitsoundPlayer::Direction direction) {
    return direction == keybinder::HitsoundPlayer::Direction::Outgoing ? 0u : 1u;
}

const char* DirectionName(keybinder::HitsoundPlayer::Direction direction) {
    return direction == keybinder::HitsoundPlayer::Direction::Outgoing ? "outgoing" : "incoming";
}

bool ReadUInt32LE(const std::uint8_t* data, std::size_t size, std::uint32_t& value) {
    if (size < 4) {
        return false;
    }

    value = static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);
    return true;
}

#ifdef KEYBINDER_TESTING
keybinder::HitsoundPlayer::TestingHooks g_testingHooks;
#endif

} // namespace

namespace keybinder {

struct HitsoundPlayer::SlotState {
    bool enabled = true;
    bool loaded = false;
    bool useEmbeddedDefault = true;
    int defaultResourceId = 0;
    float volume = 0.08f;
    std::string logicalPath;
    std::string resolvedPath;
    std::string sourceKey;
    bool hasLastPlayed = false;
    std::chrono::steady_clock::time_point lastPlayedAt{};

#ifndef KEYBINDER_TESTING
    struct SoundAsset {
        std::vector<std::uint8_t> formatBytes;
        std::vector<std::uint8_t> audioBytes;

        void Clear() {
            formatBytes.clear();
            audioBytes.clear();
        }

        bool Empty() const {
            return formatBytes.empty() || audioBytes.empty();
        }

        const WAVEFORMATEX* Format() const {
            return reinterpret_cast<const WAVEFORMATEX*>(formatBytes.data());
        }
    } asset;

    struct VoiceEntry {
        struct Callback final : IXAudio2VoiceCallback {
            VoiceEntry* owner = nullptr;

            STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) override {}
            STDMETHOD_(void, OnVoiceProcessingPassEnd)() override {}
            STDMETHOD_(void, OnStreamEnd)() override {}
            STDMETHOD_(void, OnBufferStart)(void*) override {}
            STDMETHOD_(void, OnLoopEnd)(void*) override {}
            STDMETHOD_(void, OnVoiceError)(void*, HRESULT) override {}

            STDMETHOD_(void, OnBufferEnd)(void*) override {
                if (owner != nullptr) {
                    owner->bufferEnded.store(true);
                }
            }
        } callback;

        IXAudio2SourceVoice* voice = nullptr;
        std::atomic<bool> bufferEnded{false};
        bool inUse = false;

        VoiceEntry() {
            callback.owner = this;
        }
    };

    std::vector<std::unique_ptr<VoiceEntry>> voices;
#endif
};

namespace {

#ifndef KEYBINDER_TESTING
HMODULE GetCurrentModuleHandle() {
    HMODULE module = nullptr;
    if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&GetCurrentModuleHandle),
            &module) == 0) {
        return nullptr;
    }

    return module;
}

void DestroyVoicePool(HitsoundPlayer::SlotState& slot) {
    for (auto& voiceEntry : slot.voices) {
        if (voiceEntry->voice != nullptr) {
            voiceEntry->voice->Stop(0);
            voiceEntry->voice->FlushSourceBuffers();
            voiceEntry->voice->DestroyVoice();
            voiceEntry->voice = nullptr;
        }
    }

    slot.voices.clear();
}

void RecycleCompletedVoices(HitsoundPlayer::SlotState& slot) {
    for (auto& voiceEntry : slot.voices) {
        if (!voiceEntry->inUse || !voiceEntry->bufferEnded.load()) {
            continue;
        }

        voiceEntry->voice->Stop(0);
        voiceEntry->voice->FlushSourceBuffers();
        voiceEntry->bufferEnded.store(false);
        voiceEntry->inUse = false;
    }
}

bool ParseWaveBytes(
    const std::uint8_t* bytes,
    std::size_t size,
    HitsoundPlayer::SlotState::SoundAsset& asset,
    const char* sourceDescription) {
    asset.Clear();

    if (bytes == nullptr || size < 12) {
        Logger::Warning("[HitsoundPlayer] Invalid WAV header for %s", sourceDescription);
        return false;
    }

    if (std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0) {
        Logger::Warning("[HitsoundPlayer] Unsupported WAV container for %s", sourceDescription);
        return false;
    }

    std::size_t offset = 12;
    bool foundFmt = false;
    bool foundData = false;

    while (offset + 8 <= size) {
        const char* chunkId = reinterpret_cast<const char*>(bytes + offset);
        std::uint32_t chunkSize = 0;
        if (!ReadUInt32LE(bytes + offset + 4, size - offset - 4, chunkSize)) {
            Logger::Warning("[HitsoundPlayer] Failed to read WAV chunk size for %s", sourceDescription);
            return false;
        }

        const std::size_t chunkDataOffset = offset + 8;
        if (chunkDataOffset + static_cast<std::size_t>(chunkSize) > size) {
            Logger::Warning("[HitsoundPlayer] Truncated WAV chunk for %s", sourceDescription);
            return false;
        }

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            const std::size_t formatSize = (std::max)(static_cast<std::size_t>(sizeof(WAVEFORMATEX)), static_cast<std::size_t>(chunkSize));
            asset.formatBytes.assign(formatSize, 0);
            if (chunkSize > 0) {
                std::memcpy(asset.formatBytes.data(), bytes + chunkDataOffset, static_cast<std::size_t>(chunkSize));
            }
            foundFmt = true;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            asset.audioBytes.assign(bytes + chunkDataOffset, bytes + chunkDataOffset + static_cast<std::size_t>(chunkSize));
            foundData = true;
        }

        offset = chunkDataOffset + static_cast<std::size_t>(chunkSize) + (chunkSize & 1u);
    }

    if (!foundFmt || !foundData || asset.Empty()) {
        Logger::Warning("[HitsoundPlayer] Missing WAV fmt/data chunks for %s", sourceDescription);
        asset.Clear();
        return false;
    }

    const WAVEFORMATEX* format = asset.Format();
    if (format == nullptr || format->nChannels == 0 || format->nSamplesPerSec == 0 || format->nBlockAlign == 0) {
        Logger::Warning("[HitsoundPlayer] Invalid WAV format for %s", sourceDescription);
        asset.Clear();
        return false;
    }

    return true;
}

bool LoadResourceWave(int resourceId, HitsoundPlayer::SlotState::SoundAsset& asset, const char* sourceDescription) {
    HMODULE module = GetCurrentModuleHandle();
    if (module == nullptr) {
        Logger::Warning("[HitsoundPlayer] Failed to resolve module handle for %s", sourceDescription);
        return false;
    }

    HRSRC resource = FindResourceA(module, MAKEINTRESOURCEA(resourceId), RT_RCDATA);
    if (resource == nullptr) {
        Logger::Warning("[HitsoundPlayer] Embedded hitsound resource %d missing for %s", resourceId, sourceDescription);
        return false;
    }

    HGLOBAL loaded = LoadResource(module, resource);
    if (loaded == nullptr) {
        Logger::Warning("[HitsoundPlayer] LoadResource failed for %s", sourceDescription);
        return false;
    }

    const DWORD size = SizeofResource(module, resource);
    const void* locked = LockResource(loaded);
    if (locked == nullptr || size == 0) {
        Logger::Warning("[HitsoundPlayer] LockResource failed for %s", sourceDescription);
        return false;
    }

    return ParseWaveBytes(static_cast<const std::uint8_t*>(locked), static_cast<std::size_t>(size), asset, sourceDescription);
}

bool LoadFileWave(const std::string& path, HitsoundPlayer::SlotState::SoundAsset& asset, const char* sourceDescription) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        Logger::Warning("[HitsoundPlayer] Failed to open hitsound file: %s", path.c_str());
        return false;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff length = input.tellg();
    if (length <= 0) {
        Logger::Warning("[HitsoundPlayer] Hitsound file is empty: %s", path.c_str());
        return false;
    }

    input.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.read(reinterpret_cast<char*>(bytes.data()), length);
    if (!input) {
        Logger::Warning("[HitsoundPlayer] Failed to read hitsound file: %s", path.c_str());
        return false;
    }

    return ParseWaveBytes(bytes.data(), bytes.size(), asset, sourceDescription);
}

HitsoundPlayer::SlotState::VoiceEntry* AcquireVoice(IXAudio2* engine, HitsoundPlayer::SlotState& slot) {
    RecycleCompletedVoices(slot);

    for (auto& voiceEntry : slot.voices) {
        if (!voiceEntry->inUse) {
            return voiceEntry.get();
        }
    }

    if (slot.voices.size() >= 4) {
        return nullptr;
    }

    auto voiceEntry = std::make_unique<HitsoundPlayer::SlotState::VoiceEntry>();
    IXAudio2SourceVoice* sourceVoice = nullptr;
    const HRESULT hr = engine->CreateSourceVoice(&sourceVoice, slot.asset.Format(), 0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceEntry->callback);
    if (FAILED(hr) || sourceVoice == nullptr) {
        Logger::Warning("[HitsoundPlayer] CreateSourceVoice failed: 0x%08lX", static_cast<unsigned long>(hr));
        return nullptr;
    }

    voiceEntry->voice = sourceVoice;
    slot.voices.push_back(std::move(voiceEntry));
    return slot.voices.back().get();
}
#endif

} // namespace

#ifdef KEYBINDER_TESTING
void HitsoundPlayer::SetTestingHooks(const TestingHooks& hooks) {
    g_testingHooks = hooks;
}

void HitsoundPlayer::ResetTestingHooks() {
    g_testingHooks = TestingHooks{};
}
#endif

HitsoundPlayer::HitsoundPlayer()
    : outgoingSlot_(new SlotState()),
      incomingSlot_(new SlotState()),
      previewSlot_(new SlotState()) {
#ifdef KEYBINDER_TESTING
    runtimeInitializationAttempted_ = true;
    InitializeRuntime();
    ApplySettings(Settings{});
#endif
}

HitsoundPlayer::~HitsoundPlayer() {
    ShutdownRuntime();
    delete outgoingSlot_;
    delete incomingSlot_;
    delete previewSlot_;
    outgoingSlot_ = nullptr;
    incomingSlot_ = nullptr;
    previewSlot_ = nullptr;
}

void HitsoundPlayer::ApplySettings(const Settings& settings) {
    auto configureSlot = [&](Direction direction, bool enabled, const std::string& logicalPath, float volume, int defaultResourceId) {
        SlotState& slot = GetSlot(direction);
        slot.enabled = enabled;
        slot.loaded = false;
        slot.useEmbeddedDefault = logicalPath.empty();
        slot.defaultResourceId = defaultResourceId;
        slot.volume = (std::max)(0.0f, (std::min)(1.0f, volume));
        slot.logicalPath = logicalPath;
        slot.resolvedPath.clear();
        slot.sourceKey.clear();
        slot.hasLastPlayed = false;

#ifndef KEYBINDER_TESTING
        DestroyVoicePool(slot);
        slot.asset.Clear();
#endif

        slotsDirty_[ToIndex(direction)] = true;
    };

    configureSlot(Direction::Outgoing, settings.hitsound_outgoing_enabled, settings.hitsound_outgoing_path, settings.hitsound_outgoing_volume, IDR_HITSOUND_OUTGOING_DEFAULT);
    configureSlot(Direction::Incoming, settings.hitsound_incoming_enabled, settings.hitsound_incoming_path, settings.hitsound_incoming_volume, IDR_HITSOUND_INCOMING_DEFAULT);

#ifdef KEYBINDER_TESTING
    EnsureRuntimeReady();
#endif
}

void HitsoundPlayer::OnDamageEvent(const killtracker::DamageEvent& event) {
    std::lock_guard<std::mutex> lock(pendingEventsMutex_);
    pendingEvents_.push_back(event);
}

void HitsoundPlayer::OnIncomingDamage() {
    killtracker::DamageEvent event;
    event.isOutgoing = false;
    event.weaponId = 0;  // any non-self-damage id; only the direction is used here
    OnDamageEvent(event);
}

void HitsoundPlayer::Update() {
    EnsureRuntimeReady();

    std::vector<killtracker::DamageEvent> localEvents;
    {
        std::lock_guard<std::mutex> lock(pendingEventsMutex_);
        localEvents = std::move(pendingEvents_);
        pendingEvents_.clear();
    }

    for (const auto& event : localEvents) {
        ProcessEvent(event);
    }
}

void HitsoundPlayer::EnsureRuntimeReady() {
    if (!runtimeInitializationAttempted_) {
        runtimeInitializationAttempted_ = true;
        InitializeRuntime();
    }

    if (!runtimeAvailable_) {
        return;
    }

    for (Direction direction : {Direction::Outgoing, Direction::Incoming}) {
        const std::size_t index = ToIndex(direction);
        if (!slotsDirty_[index]) {
            continue;
        }

        const SlotState& slot = GetSlot(direction);
        ReloadSlot(direction, slot.enabled, slot.logicalPath, slot.volume, slot.defaultResourceId);
        slotsDirty_[index] = false;
    }
}

bool HitsoundPlayer::IsEnabled() const {
    return enabled_;
}

void HitsoundPlayer::Shutdown() {
    // Drain the queue so no late callbacks attempt playback.
    {
        std::lock_guard<std::mutex> lock(pendingEventsMutex_);
        pendingEvents_.clear();
    }

    ShutdownRuntime();
}

void HitsoundPlayer::PreviewOutgoing() {
    if (previewSlot_ == nullptr) {
        return;
    }

    const SlotState& slot = GetSlot(Direction::Outgoing);
    ReloadState(*previewSlot_, Direction::Outgoing, true, slot.logicalPath, slot.volume, slot.defaultResourceId);
    TryPlayState(*previewSlot_, Direction::Outgoing, false);
}

void HitsoundPlayer::PreviewIncoming() {
    if (previewSlot_ == nullptr) {
        return;
    }

    const SlotState& slot = GetSlot(Direction::Incoming);
    ReloadState(*previewSlot_, Direction::Incoming, true, slot.logicalPath, slot.volume, slot.defaultResourceId);
    TryPlayState(*previewSlot_, Direction::Incoming, false);
}

void HitsoundPlayer::PreviewPath(Direction direction, const std::string& logicalPath) {
    if (previewSlot_ == nullptr) {
        return;
    }

    const SlotState& slot = GetSlot(direction);
    ReloadState(*previewSlot_, direction, true, logicalPath, slot.volume, slot.defaultResourceId);
    TryPlayState(*previewSlot_, direction, false);
}

bool HitsoundPlayer::IsOutgoingEnabled() const {
    return outgoingSlot_ != nullptr && outgoingSlot_->enabled;
}

bool HitsoundPlayer::IsIncomingEnabled() const {
    return incomingSlot_ != nullptr && incomingSlot_->enabled;
}

const std::string& HitsoundPlayer::GetOutgoingPath() const {
    static const std::string empty;
    return outgoingSlot_ != nullptr ? outgoingSlot_->logicalPath : empty;
}

const std::string& HitsoundPlayer::GetIncomingPath() const {
    static const std::string empty;
    return incomingSlot_ != nullptr ? incomingSlot_->logicalPath : empty;
}

float HitsoundPlayer::GetOutgoingVolume() const {
    return outgoingSlot_ != nullptr ? outgoingSlot_->volume : 0.08f;
}

float HitsoundPlayer::GetIncomingVolume() const {
    return incomingSlot_ != nullptr ? incomingSlot_->volume : 0.08f;
}

#ifdef KEYBINDER_TESTING
std::size_t HitsoundPlayer::GetPendingEventCountForTesting() const {
    std::lock_guard<std::mutex> lock(pendingEventsMutex_);
    return pendingEvents_.size();
}

int HitsoundPlayer::GetPlayCountForTesting(Direction direction) const {
    return playCounts_[ToIndex(direction)];
}

int HitsoundPlayer::GetPreviewPlayCountForTesting() const {
    return previewPlayCount_;
}

bool HitsoundPlayer::HasLoadedSoundForTesting(Direction direction) const {
    return GetSlot(direction).loaded;
}

bool HitsoundPlayer::HasLoadedPreviewSoundForTesting() const {
    return previewSlot_ != nullptr && previewSlot_->loaded;
}

bool HitsoundPlayer::IsRuntimeAvailableForTesting() const {
    return runtimeAvailable_;
}

float HitsoundPlayer::GetVolumeForTesting(Direction direction) const {
    return GetSlot(direction).volume;
}

float HitsoundPlayer::GetPreviewVolumeForTesting() const {
    return previewSlot_ != nullptr ? previewSlot_->volume : 0.08f;
}

const std::string& HitsoundPlayer::GetPreviewSourceKeyForTesting() const {
    static const std::string empty;
    return previewSlot_ != nullptr ? previewSlot_->sourceKey : empty;
}
#endif

void HitsoundPlayer::InitializeRuntime() {
#ifdef KEYBINDER_TESTING
    runtimeAvailable_ = !g_testingHooks.failInitialization;
    enabled_ = runtimeAvailable_;
    if (!runtimeAvailable_) {
        Logger::Warning("[HitsoundPlayer] XAudio2 initialization failed; hitsounds disabled");
    }
#else
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(comResult)) {
        comInitialized_ = true;
    } else if (comResult != RPC_E_CHANGED_MODE) {
        Logger::Warning("[HitsoundPlayer] CoInitializeEx failed: 0x%08lX", static_cast<unsigned long>(comResult));
        DisableRuntime("CoInitializeEx failed");
        return;
    }

    HRESULT hr = XAudio2Create(&engine_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr) || engine_ == nullptr) {
        Logger::Warning("[HitsoundPlayer] XAudio2Create failed: 0x%08lX", static_cast<unsigned long>(hr));
        DisableRuntime("XAudio2Create failed");
        return;
    }

    hr = engine_->CreateMasteringVoice(&masteringVoice_);
    if (FAILED(hr) || masteringVoice_ == nullptr) {
        Logger::Warning("[HitsoundPlayer] CreateMasteringVoice failed: 0x%08lX", static_cast<unsigned long>(hr));
        engine_->Release();
        engine_ = nullptr;
        DisableRuntime("CreateMasteringVoice failed");
        return;
    }

    runtimeAvailable_ = true;
    enabled_ = true;
#endif
}

void HitsoundPlayer::ShutdownRuntime() {
#ifndef KEYBINDER_TESTING
    if (outgoingSlot_ != nullptr) {
        DestroyVoicePool(*outgoingSlot_);
        outgoingSlot_->asset.Clear();
    }
    if (incomingSlot_ != nullptr) {
        DestroyVoicePool(*incomingSlot_);
        incomingSlot_->asset.Clear();
    }
    if (previewSlot_ != nullptr) {
        DestroyVoicePool(*previewSlot_);
        previewSlot_->asset.Clear();
    }

    if (masteringVoice_ != nullptr) {
        masteringVoice_->DestroyVoice();
        masteringVoice_ = nullptr;
    }
    if (engine_ != nullptr) {
        engine_->Release();
        engine_ = nullptr;
    }
#endif

    runtimeAvailable_ = false;
    enabled_ = false;

    if (comInitialized_) {
        CoUninitialize();
        comInitialized_ = false;
    }
}

void HitsoundPlayer::DisableRuntime(const char* reason) {
    Logger::Warning("[HitsoundPlayer] Disabling hitsound runtime: %s", reason);
    enabled_ = false;
    runtimeAvailable_ = false;
}

void HitsoundPlayer::ReloadState(SlotState& slot, Direction direction, bool enabled, const std::string& logicalPath, float volume, int defaultResourceId) {
    slot.enabled = enabled;
    slot.loaded = false;
    slot.useEmbeddedDefault = logicalPath.empty();
    slot.defaultResourceId = defaultResourceId;
    slot.volume = (std::max)(0.0f, (std::min)(1.0f, volume));
    slot.logicalPath = logicalPath;
    slot.resolvedPath.clear();
    slot.sourceKey.clear();
    slot.hasLastPlayed = false;

#ifndef KEYBINDER_TESTING
    DestroyVoicePool(slot);
    slot.asset.Clear();
#endif

    if (!slot.enabled || !enabled_ || !runtimeAvailable_) {
        return;
    }

    if (slot.useEmbeddedDefault) {
        slot.sourceKey = std::string("embedded:") + DirectionName(direction);
#ifdef KEYBINDER_TESTING
        const bool canLoad = g_testingHooks.canLoadSource ? g_testingHooks.canLoadSource(direction, slot.sourceKey, true) : true;
        if (!canLoad) {
            Logger::Warning("[HitsoundPlayer] Failed to load embedded %s hitsound", DirectionName(direction));
            return;
        }
        slot.loaded = true;
#else
        slot.loaded = LoadResourceWave(defaultResourceId, slot.asset, slot.sourceKey.c_str());
#endif
        if (!slot.loaded) {
            Logger::Warning("[HitsoundPlayer] Failed to load embedded %s hitsound", DirectionName(direction));
        }
        return;
    }

    slot.resolvedPath = PersistentPaths::ResolveAssetPath(logicalPath);
#ifdef KEYBINDER_TESTING
    if (slot.resolvedPath.empty()) {
        slot.resolvedPath = logicalPath;
    }
#endif
    slot.sourceKey = slot.resolvedPath.empty() ? logicalPath : slot.resolvedPath;

    if (slot.resolvedPath.empty()) {
        Logger::Warning("[HitsoundPlayer] Failed to resolve %s hitsound path: %s", DirectionName(direction), logicalPath.c_str());
        return;
    }

#ifdef KEYBINDER_TESTING
    const bool canLoad = g_testingHooks.canLoadSource ? g_testingHooks.canLoadSource(direction, slot.sourceKey, false) : true;
    if (!canLoad) {
        Logger::Warning("[HitsoundPlayer] Failed to load custom %s hitsound: %s", DirectionName(direction), logicalPath.c_str());
        return;
    }
    slot.loaded = true;
#else
    slot.loaded = LoadFileWave(slot.resolvedPath, slot.asset, slot.resolvedPath.c_str());
#endif

    if (!slot.loaded) {
        Logger::Warning("[HitsoundPlayer] Failed to load custom %s hitsound: %s", DirectionName(direction), logicalPath.c_str());
    }
}

void HitsoundPlayer::ReloadSlot(Direction direction, bool enabled, const std::string& logicalPath, float volume, int defaultResourceId) {
    ReloadState(GetSlot(direction), direction, enabled, logicalPath, volume, defaultResourceId);
}

void HitsoundPlayer::ProcessEvent(const killtracker::DamageEvent& event) {
    if (!enabled_ || !runtimeAvailable_) {
        return;
    }

    // SA-MP reports fall/self damage on the same damage RPC with weapon 54.
    // Hitsounds should only play for actual combat damage between players.
    if (event.weaponId == kSelfDamageWeaponId) {
        return;
    }

    TryPlay(event.isOutgoing ? Direction::Outgoing : Direction::Incoming);
}

bool HitsoundPlayer::TryPlay(Direction direction) {
    return TryPlayState(GetSlot(direction), direction, true);
}

bool HitsoundPlayer::TryPlayState(SlotState& slot, Direction direction, bool enforceCooldown) {
    EnsureRuntimeReady();

    if (!slot.enabled || !slot.loaded) {
        return false;
    }

    const auto now = Now();
    if (enforceCooldown && slot.hasLastPlayed && now - slot.lastPlayedAt < kDirectionCooldown) {
        return false;
    }

#ifdef KEYBINDER_TESTING
    if (enforceCooldown) {
        slot.hasLastPlayed = true;
        slot.lastPlayedAt = now;
        ++playCounts_[ToIndex(direction)];
    } else {
        ++previewPlayCount_;
    }
    return true;
#else
    if (engine_ == nullptr || masteringVoice_ == nullptr || slot.asset.Empty()) {
        return false;
    }

    SlotState::VoiceEntry* voiceEntry = AcquireVoice(engine_, slot);
    if (voiceEntry == nullptr || voiceEntry->voice == nullptr) {
        Logger::Warning("[HitsoundPlayer] No source voice available for %s hitsound", DirectionName(direction));
        return false;
    }

    XAUDIO2_BUFFER buffer = {};
    buffer.AudioBytes = static_cast<UINT32>(slot.asset.audioBytes.size());
    buffer.pAudioData = slot.asset.audioBytes.data();
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    voiceEntry->voice->Stop(0);
    voiceEntry->voice->FlushSourceBuffers();
    voiceEntry->bufferEnded.store(false);

    HRESULT hr = voiceEntry->voice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr)) {
        Logger::Warning("[HitsoundPlayer] SubmitSourceBuffer failed: 0x%08lX", static_cast<unsigned long>(hr));
        DisableRuntime("SubmitSourceBuffer failed");
        return false;
    }

    hr = voiceEntry->voice->SetVolume(slot.volume);
    if (FAILED(hr)) {
        Logger::Warning("[HitsoundPlayer] SourceVoice::SetVolume failed: 0x%08lX", static_cast<unsigned long>(hr));
        DisableRuntime("SourceVoice::SetVolume failed");
        return false;
    }

    hr = voiceEntry->voice->Start(0);
    if (FAILED(hr)) {
        Logger::Warning("[HitsoundPlayer] SourceVoice::Start failed: 0x%08lX", static_cast<unsigned long>(hr));
        DisableRuntime("SourceVoice::Start failed");
        return false;
    }

    voiceEntry->inUse = true;
    if (enforceCooldown) {
        slot.hasLastPlayed = true;
        slot.lastPlayedAt = now;
    }
    return true;
#endif
}

HitsoundPlayer::SlotState& HitsoundPlayer::GetSlot(Direction direction) {
    return direction == Direction::Outgoing ? *outgoingSlot_ : *incomingSlot_;
}

const HitsoundPlayer::SlotState& HitsoundPlayer::GetSlot(Direction direction) const {
    return direction == Direction::Outgoing ? *outgoingSlot_ : *incomingSlot_;
}

std::chrono::steady_clock::time_point HitsoundPlayer::Now() const {
#ifdef KEYBINDER_TESTING
    if (g_testingHooks.nowProvider) {
        return g_testingHooks.nowProvider();
    }
#endif
    return std::chrono::steady_clock::now();
}

} // namespace keybinder
