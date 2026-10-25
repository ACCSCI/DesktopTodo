#include "todo/windows.hpp"
#include <shlobj.h>
#include <stdexcept>

namespace todo::win {
std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (!count) throw std::invalid_argument("Invalid UTF-8 text");
    std::wstring result(std::size_t(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), result.data(), count);
    return result;
}
std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    if (!count) throw std::invalid_argument("Invalid UTF-16 text");
    std::string result(std::size_t(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), int(text.size()), result.data(), count, nullptr, nullptr);
    return result;
}
std::wstring executable_path() {
    std::wstring result(32768, L'\0');
    DWORD count = GetModuleFileNameW(nullptr, result.data(), DWORD(result.size()));
    if (!count || count == result.size()) throw std::runtime_error(last_error("Executable path"));
    result.resize(count);
    return result;
}
std::filesystem::path default_data_directory() {
    return portable_data_directory(executable_path());
}
std::filesystem::path portable_data_directory(const std::filesystem::path& executable) {
    return executable.parent_path();
}
std::filesystem::path legacy_data_directory() {
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local)))
        throw std::runtime_error("Cannot locate LocalAppData");
    std::filesystem::path directory(local);
    CoTaskMemFree(local);
    directory /= L"DesktopTodo";
    return directory;
}
std::string path_utf8(const std::filesystem::path& path) { return utf8(path.wstring()); }
std::string last_error(const char* action, DWORD error) {
    wchar_t* message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, error, 0, reinterpret_cast<LPWSTR>(&message), 0, nullptr);
    std::string result = action + std::string(": ") + (message ? utf8(message) : std::to_string(error));
    if (message) LocalFree(message);
    return result;
}
COLORREF color(Color value) { return RGB((value >> 16) & 255, (value >> 8) & 255, value & 255); }
Color color_value(COLORREF value) { return (Color(GetRValue(value)) << 16) | (Color(GetGValue(value)) << 8) | GetBValue(value); }
}
