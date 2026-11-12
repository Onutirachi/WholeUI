// PURE: C++ standard library only (compiled into the mod and the unit tests).
//
// A thread-safe list of listeners, used by the compatibility engine (FR-019) and the design
// tokens (contracts/engine-api.md). A listener is a plain function pointer plus a context
// pointer; the context is passed as the last argument.
//
// Notify copies the list under the lock and calls the listeners after releasing it, so a
// listener may add or remove listeners (itself included) without deadlocking. A listener
// added during a notification is not called by that notification. A listener removed during
// a notification is not called afterwards by that notification on the removing thread; a
// call already running on another thread still completes.

#ifndef WHOLEUI_CORE_SUBSCRIPTIONS_H
#define WHOLEUI_CORE_SUBSCRIPTIONS_H

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <vector>

namespace wholeui {

// 0 is never a valid id.
using SubscriptionId = std::uint64_t;
inline constexpr SubscriptionId kNoSubscription = 0;

template <typename... Args>
class ListenerList {
public:
    using Fn = void (*)(Args..., void* context);

    // Returns kNoSubscription when fn is null.
    SubscriptionId Add(Fn fn, void* context) {
        if (fn == nullptr) {
            return kNoSubscription;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        SubscriptionId id = ++lastId_;
        entries_.push_back(Entry{id, fn, context});
        return id;
    }

    // Returns false when id is not registered (already removed or never added).
    bool Remove(SubscriptionId id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(entries_.begin(), entries_.end(),
                               [id](const Entry& e) { return e.id == id; });
        if (it == entries_.end()) {
            return false;
        }
        entries_.erase(it);
        return true;
    }

    std::size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

    // Calls every listener registered when Notify starts, in the order they were added.
    void Notify(Args... args) const {
        std::vector<Entry> snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = entries_;
        }
        for (const Entry& entry : snapshot) {
            if (!IsRegistered(entry.id)) {
                continue;
            }
            entry.fn(args..., entry.context);
        }
    }

private:
    struct Entry {
        SubscriptionId id;
        Fn fn;
        void* context;
    };

    bool IsRegistered(SubscriptionId id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::any_of(entries_.begin(), entries_.end(),
                           [id](const Entry& e) { return e.id == id; });
    }

    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
    SubscriptionId lastId_ = kNoSubscription;
};

}  // namespace wholeui

#endif  // WHOLEUI_CORE_SUBSCRIPTIONS_H
