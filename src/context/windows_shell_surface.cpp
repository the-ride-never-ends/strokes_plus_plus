#include "context/windows_shell_surface.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>

namespace strokes::context {

bool is_protected_shell_class(std::wstring_view class_name) noexcept {
  // Windows 10 and older use NotifyIconOverflowWindow. Windows 11 hosts the same flyout in a
  // XAML island with TopLevelWindowForOverflowXamlIsland as its top-level HWND.
  constexpr std::array protected_classes{
      std::wstring_view{L"Shell_TrayWnd"},
      std::wstring_view{L"Shell_SecondaryTrayWnd"},
      std::wstring_view{L"NotifyIconOverflowWindow"},
      std::wstring_view{L"TopLevelWindowForOverflowXamlIsland"},
  };
  for (const auto candidate : protected_classes) {
    if (class_name == candidate) return true;
  }
  return false;
}

bool is_protected_shell_window(std::uintptr_t window) noexcept {
  HWND handle = reinterpret_cast<HWND>(window);
  if (handle == nullptr || !::IsWindow(handle)) return false;

  // Inspect both the hit-tested window and its ancestors. This covers classic child controls and
  // newer XAML-hosted taskbar content without classifying every generic XAML window as shell UI.
  for (HWND current = handle; current != nullptr; current = ::GetParent(current)) {
    wchar_t class_name[128]{};
    const int length = ::GetClassNameW(current, class_name, static_cast<int>(std::size(class_name)));
    if (length > 0 && is_protected_shell_class(
                          std::wstring_view{class_name, static_cast<std::size_t>(length)})) {
      return true;
    }
  }
  return false;
}

}  // namespace strokes::context
