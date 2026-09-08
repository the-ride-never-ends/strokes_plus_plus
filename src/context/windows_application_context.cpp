#include "context/windows_application_context.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace strokes::context {
namespace {

class UniqueHandle {
public:
    explicit UniqueHandle(HANDLE handle = nullptr) noexcept : handle_(handle) {}
    ~UniqueHandle() {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(handle_);
        }
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] explicit operator bool() const noexcept {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE handle_;
};

std::string utf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int required = ::WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    const int written = ::WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        result.data(), required, nullptr, nullptr);
    return written == required ? result : std::string{};
}

std::string window_title(HWND window) {
    const int length = ::GetWindowTextLengthW(window);
    if (length <= 0) {
        return {};
    }
    std::wstring title(static_cast<std::size_t>(length) + 1, L'\0');
    const int copied = ::GetWindowTextW(window, title.data(), static_cast<int>(title.size()));
    if (copied <= 0) {
        return {};
    }
    title.resize(static_cast<std::size_t>(copied));
    return utf8(title);
}

std::string window_class(HWND window) {
    std::vector<wchar_t> buffer(256);
    for (;;) {
        const int copied = ::GetClassNameW(window, buffer.data(), static_cast<int>(buffer.size()));
        if (copied <= 0) {
            return {};
        }
        if (static_cast<std::size_t>(copied) + 1 < buffer.size() || buffer.size() >= 65536) {
            return utf8({buffer.data(), static_cast<std::size_t>(copied)});
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::string executable_name(DWORD process_id) {
    const UniqueHandle process(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id));
    if (!process) {
        return {};
    }

    std::vector<wchar_t> path(32768);
    DWORD length = static_cast<DWORD>(path.size());
    if (!::QueryFullProcessImageNameW(process.get(), 0, path.data(), &length)) {
        return {};
    }
    const std::filesystem::path executable(std::wstring_view(path.data(), length));
    return utf8(executable.filename().native());
}

}  // namespace

std::optional<ApplicationContext> WindowsApplicationContextProvider::foreground_application() const {
    const HWND window = ::GetForegroundWindow();
    if (window == nullptr) {
        return std::nullopt;
    }

    DWORD process_id = 0;
    if (::GetWindowThreadProcessId(window, &process_id) == 0 || process_id == 0) {
        return std::nullopt;
    }

    ApplicationContext result;
    result.window_handle = reinterpret_cast<std::uintptr_t>(window);
    result.process_id = process_id;
    result.executable_name = executable_name(process_id);
    result.window_title = window_title(window);
    result.window_class = window_class(window);
    return result;
}

}  // namespace strokes::context
