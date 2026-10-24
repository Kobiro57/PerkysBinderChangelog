#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace RakNet {
class BitStream;
}

namespace keybinder {

struct TextDrawAdapterState;

class TextDrawAdapter {
public:
    TextDrawAdapter();
    ~TextDrawAdapter();

    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return initialized_.load(); }

    void SetSpeedometerEnabled(bool enabled);
    void SetVehicleContext(bool inVehicle);
    void SetDiagnosticsEnabled(bool enabled);
    bool IsDiagnosticsEnabled() const;
    unsigned long GetDiagnosticsEventCount() const;
    bool IsRenderSuppressionReady() const;

    std::string GetFuelText() const;
    float GetFuelValue() const;

    bool DumpAllTextDrawsToFile(const std::string& outputPath, std::size_t* dumpedCount = nullptr) const;

#ifdef KEYBINDER_TESTING
    static void RenderVisibilityMaskForTesting(int* visible, void** objects, std::size_t count,
        const std::vector<int>& hiddenIds, const std::function<void()>& draw);
    static std::string StripColorCodesForTesting(const std::string& text);
    static float ParseFuelValueForTesting(const std::string& text);
    static bool TryParseRpcTextForTesting(unsigned char rpcId, RakNet::BitStream* bs, int& textDrawId, std::string& text);
    static bool TextDrawIdMatchesForTesting(int rpcId);
    static bool ShouldSuppressShowTextDrawForTesting(
        int rpcId,
        bool speedometerEnabled,
        bool inVehicleContext,
        float posX,
        float posY,
        int style,
        int box,
        int center,
        float boxSizeX,
        float boxSizeY,
        float letterWidth,
        float letterHeight,
        const std::string& rawText);
    static bool ShouldSuppressSetStringTextDrawForTesting(
        int rpcId,
        bool speedometerEnabled,
        bool inVehicleContext = true,
        const std::string& rawText = std::string());
    static bool ShouldSuppressSetStringTextDrawWithSnapshotForTesting(
        int rpcId,
        bool speedometerEnabled,
        bool inVehicleContext,
        float posX,
        float posY,
        int style,
        int box,
        int center,
        float boxSizeX,
        float boxSizeY,
        float letterWidth,
        float letterHeight,
        const std::string& currentRawText,
        const std::string& setStringRawText);
    static bool ShouldCacheFuelTextForTesting(
        int rpcId,
        bool speedometerEnabled,
        bool inVehicleContext,
        const std::string& rawText);
    static bool LooksLikeSpeedometerTextDrawForTesting(
        int rpcId,
        float posX,
        float posY,
        int style,
        int box,
        int center,
        float boxSizeX,
        float boxSizeY,
        float letterWidth,
        float letterHeight,
        const std::string& rawText);
    static std::string ClassifyShowTextDrawRoleForTesting(
        int rpcId,
        float posX,
        float posY,
        int style,
        int box,
        int center,
        float boxSizeX,
        float boxSizeY,
        float letterWidth,
        float letterHeight,
        const std::string& rawText);
    static int ClassifyShowTextDrawConfidenceForTesting(
        int rpcId,
        float posX,
        float posY,
        int style,
        int box,
        int center,
        float boxSizeX,
        float boxSizeY,
        float letterWidth,
        float letterHeight,
        const std::string& rawText);
    static bool ShouldSuppressSetStringAfterShowForTesting(
        int showRpcId,
        float posX,
        float posY,
        int style,
        int box,
        int center,
        float boxSizeX,
        float boxSizeY,
        float letterWidth,
        float letterHeight,
        const std::string& showRawText,
        int setStringRpcId,
        bool speedometerEnabled,
        bool inVehicleContext,
        const std::string& setStringRawText);
    static bool ShouldSuppressSetStringAfterShowContextChangeForTesting(
        int showRpcId,
        float posX,
        float posY,
        int style,
        int box,
        int center,
        float boxSizeX,
        float boxSizeY,
        float letterWidth,
        float letterHeight,
        const std::string& showRawText,
        bool showSpeedometerEnabled,
        bool showInVehicleContext,
        int setStringRpcId,
        bool setStringSpeedometerEnabled,
        bool setStringInVehicleContext,
        const std::string& setStringRawText);
    static bool ShouldSuppressSetStringAfterShowThenHideForTesting(
        int showRpcId,
        float posX,
        float posY,
        int style,
        int box,
        int center,
        float boxSizeX,
        float boxSizeY,
        float letterWidth,
        float letterHeight,
        const std::string& showRawText,
        int hideRpcId,
        int setStringRpcId,
        bool speedometerEnabled,
        bool inVehicleContext,
        const std::string& setStringRawText);
#endif

private:
    std::atomic<bool> initialized_{false};
    std::shared_ptr<TextDrawAdapterState> state_;
};

} // namespace keybinder
