#include "../../src/core/subscriptions.h"

#include <atomic>
#include <thread>
#include <vector>

#include "harness.h"

using wholeui::kNoSubscription;
using wholeui::ListenerList;
using wholeui::SubscriptionId;

namespace {

struct Counter {
    int calls = 0;
    int lastValue = 0;
};

void Count(int value, void* context) {
    auto* counter = static_cast<Counter*>(context);
    counter->calls++;
    counter->lastValue = value;
}

}  // namespace

TEST(subscriptions_add_and_notify) {
    ListenerList<int> list;
    Counter a, b;
    SubscriptionId idA = list.Add(&Count, &a);
    SubscriptionId idB = list.Add(&Count, &b);
    CHECK(idA != kNoSubscription);
    CHECK(idB != kNoSubscription);
    CHECK(idA != idB);
    CHECK_EQ(list.Size(), 2u);

    list.Notify(7);
    CHECK_EQ(a.calls, 1);
    CHECK_EQ(a.lastValue, 7);
    CHECK_EQ(b.calls, 1);
    CHECK_EQ(b.lastValue, 7);
}

TEST(subscriptions_add_null_is_rejected) {
    ListenerList<int> list;
    CHECK_EQ(list.Add(nullptr, nullptr), kNoSubscription);
    CHECK_EQ(list.Size(), 0u);
}

TEST(subscriptions_remove) {
    ListenerList<int> list;
    Counter a, b;
    SubscriptionId idA = list.Add(&Count, &a);
    list.Add(&Count, &b);

    CHECK(list.Remove(idA));
    CHECK(!list.Remove(idA));
    CHECK(!list.Remove(kNoSubscription));
    list.Notify(1);
    CHECK_EQ(a.calls, 0);
    CHECK_EQ(b.calls, 1);
}

TEST(subscriptions_clear) {
    ListenerList<int> list;
    Counter a, b;
    SubscriptionId idA = list.Add(&Count, &a);
    list.Add(&Count, &b);

    list.Clear();
    CHECK_EQ(list.Size(), 0u);
    CHECK(!list.Remove(idA));
    list.Notify(1);
    CHECK_EQ(a.calls, 0);
    CHECK_EQ(b.calls, 0);

    SubscriptionId again = list.Add(&Count, &a);
    CHECK(again != idA);
}

TEST(subscriptions_ids_are_not_reused) {
    ListenerList<int> list;
    Counter a;
    SubscriptionId first = list.Add(&Count, &a);
    list.Remove(first);
    SubscriptionId second = list.Add(&Count, &a);
    CHECK(first != second);
}

// A listener that removes itself, and another one, from inside the callback. If Notify held
// the lock while calling, Remove would deadlock here.
struct SelfRemover {
    ListenerList<int>* list = nullptr;
    SubscriptionId self = kNoSubscription;
    SubscriptionId other = kNoSubscription;
    int calls = 0;
};

static void RemoveSelfAndOther(int, void* context) {
    auto* r = static_cast<SelfRemover*>(context);
    r->calls++;
    r->list->Remove(r->self);
    r->list->Remove(r->other);
}

TEST(subscriptions_remove_from_inside_callback) {
    ListenerList<int> list;
    SelfRemover remover;
    Counter other;
    remover.list = &list;
    remover.self = list.Add(&RemoveSelfAndOther, &remover);
    remover.other = list.Add(&Count, &other);

    list.Notify(1);
    CHECK_EQ(remover.calls, 1);
    CHECK_EQ(other.calls, 0);  // removed before its turn in the same notification
    CHECK_EQ(list.Size(), 0u);

    list.Notify(2);
    CHECK_EQ(remover.calls, 1);
}

// A listener that adds another one from inside the callback: the new listener is not part of
// the running notification (snapshot), and is called by the next one.
struct Adder {
    ListenerList<int>* list = nullptr;
    Counter* added = nullptr;
    bool done = false;
};

static void AddAnother(int, void* context) {
    auto* adder = static_cast<Adder*>(context);
    if (!adder->done) {
        adder->done = true;
        adder->list->Add(&Count, adder->added);
    }
}

TEST(subscriptions_callbacks_run_outside_lock_on_snapshot) {
    ListenerList<int> list;
    Counter added;
    Adder adder{&list, &added};
    list.Add(&AddAnother, &adder);

    list.Notify(1);
    CHECK(adder.done);
    CHECK_EQ(added.calls, 0);
    CHECK_EQ(list.Size(), 2u);

    list.Notify(2);
    CHECK_EQ(added.calls, 1);
    CHECK_EQ(added.lastValue, 2);
}

static void CountNoArgs(void* context) {
    static_cast<std::atomic<int>*>(context)->fetch_add(1);
}

TEST(subscriptions_concurrent_add_remove_notify) {
    ListenerList<> list;
    std::atomic<int> calls{0};
    list.Add(&CountNoArgs, &calls);

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&list, &calls] {
            for (int i = 0; i < 1000; i++) {
                SubscriptionId id = list.Add(&CountNoArgs, &calls);
                list.Notify();
                list.Remove(id);
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    CHECK_EQ(list.Size(), 1u);
    CHECK(calls.load() >= 4000);
}
