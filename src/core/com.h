// A minimal owner of one COM reference, shared by the modules that use COM or WinRT objects
// (tokens/system, diagnostic/overlay).

#ifndef WHOLEUI_CORE_COM_H
#define WHOLEUI_CORE_COM_H

namespace wholeui {

// Owns one COM reference.
template <typename T>
class ComRef {
public:
    ComRef() = default;
    ComRef(const ComRef&) = delete;
    ComRef& operator=(const ComRef&) = delete;
    ~ComRef() { Reset(); }

    T* Get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    T** Put() noexcept {
        Reset();
        return &ptr_;
    }
    void** PutVoid() noexcept { return reinterpret_cast<void**>(Put()); }
    void Reset() noexcept {
        if (ptr_ != nullptr) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

private:
    T* ptr_ = nullptr;
};

}  // namespace wholeui

#endif  // WHOLEUI_CORE_COM_H
