#pragma once

#include "safetyhook.hpp"
#include <bit>
#include <cstdint>

namespace cyanide {

// Forward declaration - specialized for the two calling conventions RakHook uses
template <typename Fn, typename Callback>
class polyhook_x86;

// Specialization for __cdecl hooks: void(__cdecl*)(void*)
// Used by destroy_rakclient_interface
template <typename Ret, typename... Args, typename Callback>
class polyhook_x86<Ret(__cdecl*)(Args...), Callback> {
public:
    using Fn = Ret(__cdecl*)(Args...);

    polyhook_x86(Fn original, Callback callback)
        : original_(original), callback_(callback) {
        instance_ = this;
    }

    ~polyhook_x86() {
        if (instance_ == this) instance_ = nullptr;
        hook_.reset();
    }

    bool install() {
        if (installed_) return true; // Already hooked
        auto result = safetyhook::InlineHook::create(
            reinterpret_cast<void*>(original_),
            reinterpret_cast<void*>(&thunk)
        );
        if (result.has_value()) {
            hook_ = std::move(*result);
            installed_ = true;
            return true;
        }
        return false;
    }

    void* get_trampoline() const {
        if (hook_) {
            return reinterpret_cast<void*>(hook_.trampoline().address());
        }
        return nullptr;
    }

    explicit operator bool() const { return installed_; }

private:
    Fn original_;
    Callback callback_;
    safetyhook::InlineHook hook_;
    bool installed_ = false;

    static inline polyhook_x86* instance_ = nullptr;

    static Ret __cdecl thunk(Args... args) {
        if (instance_ && instance_->hook_) {
            Fn trampoline = reinterpret_cast<Fn>(instance_->hook_.trampoline().address());
            return instance_->callback_(trampoline, args...);
        }
        return Ret();
    }
};

// Specialization for __thiscall hooks: bool(__thiscall*)(void*, const char*, int, PlayerID)
// Used by handle_rpc_packet
// On x86 MSVC, __thiscall passes 'this' in ECX and rest on stack.
// SafetyHook handles this correctly - the detour destination just needs to match the ABI.
// We use __fastcall as a workaround since free functions can't be __thiscall.
// __fastcall: first arg in ECX, second in EDX, rest on stack.
// __thiscall: this in ECX, rest on stack.
// So for __thiscall(void* this, A, B, C), the __fastcall shim is (void* this, void* edx_unused, A, B, C).
template <typename Ret, typename ThisPtr, typename... Args, typename Callback>
class polyhook_x86<Ret(__thiscall*)(ThisPtr, Args...), Callback> {
public:
    using Fn = Ret(__thiscall*)(ThisPtr, Args...);

    polyhook_x86(Fn original, Callback callback)
        : original_(original), callback_(callback) {
        instance_ = this;
    }

    ~polyhook_x86() {
        if (instance_ == this) instance_ = nullptr;
        hook_.reset();
    }

    bool install() {
        if (installed_) return true; // Already hooked
        auto result = safetyhook::InlineHook::create(
            reinterpret_cast<void*>(original_),
            reinterpret_cast<void*>(&thunk)
        );
        if (result.has_value()) {
            hook_ = std::move(*result);
            installed_ = true;
            return true;
        }
        return false;
    }

    void* get_trampoline() const {
        if (hook_) {
            return reinterpret_cast<void*>(hook_.trampoline().address());
        }
        return nullptr;
    }

    explicit operator bool() const { return installed_; }

private:
    Fn original_;
    Callback callback_;
    safetyhook::InlineHook hook_;
    bool installed_ = false;

    static inline polyhook_x86* instance_ = nullptr;

    // __fastcall shim: ECX=thisPtr, EDX=unused, then stack args
    // This matches __thiscall ABI since SafetyHook just JMPs to us
    static Ret __fastcall thunk(ThisPtr thisPtr, void* /*edx*/, Args... args) {
        if (instance_ && instance_->hook_) {
            Fn trampoline = reinterpret_cast<Fn>(instance_->hook_.trampoline().address());
            return instance_->callback_(trampoline, thisPtr, args...);
        }
        return Ret();
    }
};

} // namespace cyanide
