#include "config/windows_app_data.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>

namespace strokes::config {
std::optional<std::filesystem::path> windows_configuration_directory() {
    PWSTR value = nullptr;
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &value))) return {};
    std::filesystem::path result(value);
    ::CoTaskMemFree(value);
    return result / L"StrokesPlusPlus";
}
}
