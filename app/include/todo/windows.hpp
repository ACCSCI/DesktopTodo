#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "repository.hpp"
#include "window_policy.hpp"
#include <filesystem>

namespace todo::win {
std::wstring wide(const std::string& utf8);
std::string utf8(const std::wstring& text);
std::wstring executable_path();
std::filesystem::path default_data_directory();
std::filesystem::path portable_data_directory(const std::filesystem::path& executable);
std::filesystem::path legacy_data_directory();
bool migrate_legacy_data(const std::filesystem::path& source,const std::filesystem::path& destination);
std::filesystem::path read_storage_directory(const std::filesystem::path& base,const std::filesystem::path& settings);
void write_storage_directory(const std::filesystem::path& base,const std::filesystem::path& settings,const std::filesystem::path& directory);
std::uintptr_t data_identity(const std::filesystem::path& directory);
std::string path_utf8(const std::filesystem::path& path);
std::string last_error(const char* action, DWORD error = GetLastError());
COLORREF color(Color value);
Color color_value(COLORREF value);

class Startup final : public StartupPort {
    std::wstring executable_, value_name_, key_;
public:
    explicit Startup(std::wstring executable,
        std::wstring value_name = L"DesktopTodo",
        std::wstring key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    bool enabled() const override;
    void set_enabled(bool enabled) override;
    std::wstring command() const;
};

HWND desktop_host();
HWND desktop_surface(HWND root,HWND content);
bool is_owned(HWND window);
void apply_mode(HWND window, const Note& note, HWND desktop);
void set_screen_bounds(HWND window, const Bounds& bounds, HWND parent);
Bounds screen_bounds(HWND window);
Bounds visible_bounds(Bounds bounds);
void configure_unlock(HWND button, HWND note_window, const Note& note);
void present_layered(HWND window,HDC source,int width,int height);
enum class Glyph { lock, unlock, more, plus, grip, chevron, check, clock, note };
void draw_glyph(HDC dc,Rect area,Glyph glyph,COLORREF color);
}
