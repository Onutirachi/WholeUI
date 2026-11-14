// Scoped SRWLOCK guards. SRWLOCK is not recursive: never take a lock the thread already holds.

#ifndef WHOLEUI_CORE_LOCK_H
#define WHOLEUI_CORE_LOCK_H

#include <windows.h>

namespace wholeui {

class SharedLock {
public:
    explicit SharedLock(SRWLOCK& lock) : lock_(lock) { AcquireSRWLockShared(&lock_); }
    ~SharedLock() { ReleaseSRWLockShared(&lock_); }
    SharedLock(const SharedLock&) = delete;
    SharedLock& operator=(const SharedLock&) = delete;

private:
    SRWLOCK& lock_;
};

class ExclusiveLock {
public:
    explicit ExclusiveLock(SRWLOCK& lock) : lock_(lock) { AcquireSRWLockExclusive(&lock_); }
    ~ExclusiveLock() { ReleaseSRWLockExclusive(&lock_); }
    ExclusiveLock(const ExclusiveLock&) = delete;
    ExclusiveLock& operator=(const ExclusiveLock&) = delete;

private:
    SRWLOCK& lock_;
};

}  // namespace wholeui

#endif  // WHOLEUI_CORE_LOCK_H
