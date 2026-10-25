#include "todo/windows.hpp"
#include <stdexcept>

namespace todo::win {
Startup::Startup(std::wstring executable, std::wstring value_name, std::wstring key)
    : executable_(std::move(executable)), value_name_(std::move(value_name)), key_(std::move(key)) {
    if (executable_.empty() || executable_.find(L'"') != std::wstring::npos ||
        value_name_.empty() || key_.empty()) throw std::invalid_argument("Invalid startup configuration");
}
std::wstring Startup::command() const { return L"\"" + executable_ + L"\" --startup"; }
bool Startup::enabled() const {
    HKEY key = nullptr;
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, key_.c_str(), 0, KEY_QUERY_VALUE, &key);
    if (status == ERROR_FILE_NOT_FOUND) return false;
    if (status != ERROR_SUCCESS) throw std::runtime_error(last_error("Read startup registration", status));
    DWORD bytes = 0, type = 0;
    status = RegQueryValueExW(key, value_name_.c_str(), nullptr, &type, nullptr, &bytes);
    if (status == ERROR_FILE_NOT_FOUND) { RegCloseKey(key); return false; }
    if (status != ERROR_SUCCESS || type != REG_SZ || bytes > 65536 || bytes % sizeof(wchar_t)) {
        RegCloseKey(key);
        if (status == ERROR_SUCCESS) return false;
        throw std::runtime_error(last_error("Read startup registration", status));
    }
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    status = RegQueryValueExW(key, value_name_.c_str(), nullptr, &type,
        reinterpret_cast<BYTE*>(value.data()), &bytes);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) throw std::runtime_error(last_error("Read startup registration", status));
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return value == command();
}
void Startup::set_enabled(bool enabled) {
    if (enabled && command().size() > 259) throw std::invalid_argument("Executable path is too long for a Windows Run entry");
    HKEY key = nullptr;
    LSTATUS status = enabled ? RegCreateKeyExW(HKEY_CURRENT_USER, key_.c_str(), 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &key, nullptr) : RegOpenKeyExW(HKEY_CURRENT_USER, key_.c_str(), 0, KEY_SET_VALUE, &key);
    if (!enabled && status == ERROR_FILE_NOT_FOUND) return;
    if (status != ERROR_SUCCESS) throw std::runtime_error(last_error("Change startup registration", status));
    if (enabled) {
        auto value = command();
        status = RegSetValueExW(key, value_name_.c_str(), 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()), DWORD((value.size() + 1) * sizeof(wchar_t)));
    } else status = RegDeleteValueW(key, value_name_.c_str());
    RegCloseKey(key);
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND)
        throw std::runtime_error(last_error("Change startup registration", status));
}
}
