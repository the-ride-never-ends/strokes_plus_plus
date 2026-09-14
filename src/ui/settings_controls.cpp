#include "ui/settings_controls.h"

#include <TlHelp32.h>

#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <unordered_set>
#include <vector>

namespace strokes::ui::detail {
namespace {
constexpr wchar_t combo_height_property[] = L"StrokesPlusPlus.ComboDropHeight";

BOOL CALLBACK collect_window_process(HWND window, LPARAM context) {
  if (!::IsWindowVisible(window) || ::GetAncestor(window, GA_ROOT) != window) return TRUE;
  DWORD process_id = 0;
  (void)::GetWindowThreadProcessId(window, &process_id);
  if (process_id != 0) {
    static_cast<std::unordered_set<DWORD>*>(reinterpret_cast<void*>(context))->insert(process_id);
  }
  return TRUE;
}
}

HWND text(HWND parent, int id, const wchar_t* value, int x, int y, int w, int h) {
  static int next_label_id = 1000;
  if (id == 0) id = next_label_id++;
  return ::CreateWindowExW(0, L"STATIC", value, WS_CHILD | WS_VISIBLE | SS_NOTIFY, x, y, w, h, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
}

HWND control(HWND parent, const wchar_t* type, const wchar_t* value, DWORD style, int id, int x,
             int y, int w, int h) {
  // WS_TABSTOP is what makes IsDialogMessageW's Tab navigation reach a control.
  const bool combo = ::lstrcmpiW(type, L"COMBOBOX") == 0;
  HWND result = ::CreateWindowExW(
      WS_EX_CLIENTEDGE, type, value,
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | style | (combo ? WS_VSCROLL : 0), x, y, w, h, parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
  if (combo && result) {
    // GetWindowRect reports only the closed selection field after creation. Retain the requested
    // drop-list height so DPI scaling can restore the full list instead of collapsing it.
    (void)::SetPropW(result, combo_height_property,
                     reinterpret_cast<HANDLE>(static_cast<INT_PTR>(h)));
  }
  return result;
}

std::wstring number(double value) {
  wchar_t out[64]{};
  swprintf_s(out, L"%.6g", value);
  return out;
}

std::wstring integer(std::size_t value) { return std::to_wstring(value); }

bool read_double(HWND window, int id, double low, double high, double& output) {
  wchar_t value[128]{};
  ::GetDlgItemTextW(window, id, value, 128);
  wchar_t* end = nullptr;
  const double parsed = std::wcstod(value, &end);
  if (end == value || *end != L'\0' || parsed < low || parsed > high) return false;
  output = parsed;
  return true;
}

bool read_integer(HWND window, int id, long low, long high, long& output) {
  wchar_t value[128]{};
  ::GetDlgItemTextW(window, id, value, 128);
  wchar_t* end = nullptr;
  const long parsed = std::wcstol(value, &end, 10);
  if (end == value || *end != L'\0' || parsed < low || parsed > high) return false;
  output = parsed;
  return true;
}

std::optional<LRESULT> selected_combo(HWND window, int id, LRESULT count) {
  const LRESULT selected = ::SendDlgItemMessageW(window, id, CB_GETCURSEL, 0, 0);
  if (selected == CB_ERR || selected < 0 || selected >= count) return std::nullopt;
  return selected;
}

std::string read_utf8(HWND window, int id) {
  const int length = ::GetWindowTextLengthW(::GetDlgItem(window, id));
  if (length == 0) return {};
  std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
  ::GetDlgItemTextW(window, id, value.data(), length + 1);
  const int needed =
      ::WideCharToMultiByte(CP_UTF8, 0, value.data(), length, nullptr, 0, nullptr, nullptr);
  std::string result(static_cast<std::size_t>(needed), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, value.data(), length, result.data(), needed, nullptr, nullptr);
  return result;
}

std::wstring wide(std::string_view value) {
  if (value.empty()) return {};
  const int needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                           static_cast<int>(value.size()), nullptr, 0);
  std::wstring result(static_cast<std::size_t>(needed), L'\0');
  ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        result.data(), needed);
  return result;
}

void populate_processes(HWND combo) {
  std::unordered_set<DWORD> window_processes;
  window_processes.insert(::GetCurrentProcessId());
  (void)::EnumWindows(collect_window_process,
                      reinterpret_cast<LPARAM>(&window_processes));
  HANDLE snapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return;
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  std::vector<std::wstring> names;
  if (::Process32FirstW(snapshot, &entry)) do {
      if (window_processes.contains(entry.th32ProcessID)) names.emplace_back(entry.szExeFile);
    } while (::Process32NextW(snapshot, &entry));
  ::CloseHandle(snapshot);
  std::ranges::sort(names);
  names.erase(std::ranges::unique(names).begin(), names.end());
  (void)::SendMessageW(combo, WM_SETREDRAW, FALSE, 0);
  (void)::SendMessageW(combo, CB_INITSTORAGE, static_cast<WPARAM>(names.size()),
                       static_cast<LPARAM>(names.size() * 32));
  for (const auto& name : names)
    ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
  (void)::SendMessageW(combo, WM_SETREDRAW, TRUE, 0);
  ::InvalidateRect(combo, nullptr, TRUE);
}

}  // namespace strokes::ui::detail
