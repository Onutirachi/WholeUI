// The mod's worker thread (research R13).
//
// One thread per mod instance with a message loop and WinRT initialized single-threaded
// (RoInitialize(RO_INIT_SINGLETHREADED)). It owns the objects that need a message loop: the
// UISettings object and its event registrations (tokens) and the diagnostic overlay.
//
// Work reaches the thread through Post. Tasks run in the order they were posted, one at a
// time, on the worker thread, without any worker lock held, so a task may call Post. A task
// that throws is logged and skipped. A task must not block for long: it delays every task
// after it and the overlay's timer.
//
// Lifecycle: Start in Wh_ModInit, Stop in Wh_ModUninit. Every task posted before Stop runs
// before the thread exits, so a module can post its cleanup (for example releasing WinRT
// objects) and then rely on Stop to wait for it.

#ifndef WHOLEUI_CORE_WORKER_H
#define WHOLEUI_CORE_WORKER_H

#include <functional>

namespace wholeui::worker {

// Starts the thread. Does not wait for it: tasks posted before the thread is ready are kept
// and run once it is. Returns false (and logs a warning) if the thread cannot be created;
// Post then returns false. Calling Start while the worker runs does nothing and returns true.
bool Start() noexcept;

// Stops accepting tasks, lets the thread run the tasks already posted, then waits for it to
// exit. Must not be called from the worker thread (logged and ignored). Does nothing when the
// worker is not running.
void Stop() noexcept;

// Queues `task` to run on the worker thread. Returns false, and does not run the task, when
// the worker is not running, is stopping, or memory runs out. Safe from any thread.
bool Post(std::function<void()> task) noexcept;

// True on the worker thread.
bool IsWorkerThread() noexcept;

}  // namespace wholeui::worker

#endif  // WHOLEUI_CORE_WORKER_H
