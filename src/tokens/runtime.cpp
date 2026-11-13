// The current design tokens and their change notifications. See runtime.h.

#include "runtime.h"

#include <windows.h>

#include <atomic>
#include <memory>
#include <vector>

#include "../core/lock.h"
#include "../core/log.h"

namespace wholeui::tokens {

namespace {

// The tokens of DefaultInputs, built on first use and kept until the DLL unloads.
const Tokens& DefaultTokens() {
    static const Tokens defaults = Resolve(DefaultInputs());
    return defaults;
}

struct State {
    SRWLOCK lock = SRWLOCK_INIT;  // guards everything below except `current`
    // Every snapshot swapped in since load, oldest first. Changes are rare (a user changing
    // mode, accent or text size), so keeping them all is cheap and makes Current() safe
    // without reference counting.
    std::vector<std::unique_ptr<const Tokens>> snapshots;
    TokenInputs inputs;       // the inputs of the newest snapshot
    bool hasInputs = false;   // false until the first system read is swapped in
    std::atomic<const Tokens*> current{nullptr};  // null: the defaults
};

State g_state;
ListenerList<> g_listeners;

}  // namespace

const Tokens& Current() noexcept {
    const Tokens* current = g_state.current.load(std::memory_order_acquire);
    return current != nullptr ? *current : DefaultTokens();
}

SubscriptionId SubscribeChanged(TokensChangedFn fn, void* context) {
    return g_listeners.Add(fn, context);
}

void Unsubscribe(SubscriptionId id) {
    g_listeners.Remove(id);
}

void OnSystemInputs(const TokenInputs& inputs, void* /*context*/) noexcept {
    const Tokens* swapped = nullptr;
    try {
        {
            SharedLock lock(g_state.lock);
            if (g_state.hasInputs && g_state.inputs == inputs) {
                return;
            }
        }
        auto next = std::make_unique<const Tokens>(Resolve(inputs));
        ExclusiveLock lock(g_state.lock);
        g_state.snapshots.reserve(g_state.snapshots.size() + 1);  // may throw: nothing changed yet
        swapped = next.get();
        g_state.snapshots.push_back(std::move(next));
        g_state.inputs = inputs;
        g_state.hasInputs = true;
        g_state.current.store(swapped, std::memory_order_release);
    } catch (...) {
        log::Warn(L"tokens", L"resolving tokens failed: exception; current tokens kept");
        return;
    }
    log::Info(L"tokens", L"resolved: mode %s, accent #%06X, text scale %.2f, font %s",
              inputs.dark ? L"dark" : L"light", inputs.accent & 0x00FFFFFFu,
              static_cast<double>(inputs.textScale), swapped->fontFamily.c_str());
    try {
        g_listeners.Notify();
    } catch (...) {
        log::Warn(L"tokens", L"token change notification failed: exception");
    }
}

void ResetTokens() noexcept {
    std::vector<std::unique_ptr<const Tokens>> snapshots;  // freed after the lock
    g_listeners.Clear();
    {
        ExclusiveLock lock(g_state.lock);
        g_state.current.store(nullptr, std::memory_order_release);
        snapshots.swap(g_state.snapshots);
        g_state.inputs = TokenInputs{};
        g_state.hasInputs = false;
    }
}

}  // namespace wholeui::tokens
