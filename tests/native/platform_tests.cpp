#include "test.hpp"
#include "todo/windows.hpp"
using namespace todo;

TEST_CASE("native UTF-8 conversion round-trips Chinese and supplementary-plane icons") {
    const std::string text = "项目待办 🥛 🎯";
    CHECK(win::utf8(win::wide(text)) == text);
    THROWS_AS(win::wide(std::string("\xFF", 1)), std::invalid_argument);
    for (Color value : {Color(0), Color(0xFFFFFF), Color(0x12ABEF)}) CHECK(win::color_value(win::color(value)) == value);
}

TEST_CASE("startup registration quotes spaces and round-trips in an isolated non-startup key") {
    const auto key = L"Software\\DesktopTodo\\Tests\\Startup-" + std::to_wstring(GetCurrentProcessId());
    struct Cleanup { std::wstring key; ~Cleanup() { RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str()); } } cleanup{key};
    win::Startup startup(L"C:\\App Folder\\DesktopTodo.exe", L"TestEntry", key);
    CHECK(startup.command() == L"\"C:\\App Folder\\DesktopTodo.exe\" --startup");
    CHECK(!startup.enabled());
    startup.set_enabled(true);
    CHECK(startup.enabled());
    win::Startup other(L"C:\\Other.exe", L"TestEntry", key);
    CHECK(!other.enabled());
    startup.set_enabled(false);
    CHECK(!startup.enabled());
    startup.set_enabled(false);
}

TEST_CASE("invalid or overly long startup paths cannot create a Run entry") {
    THROWS_AS(win::Startup invalid(L"C:\\bad\"path.exe"), std::invalid_argument);
    win::Startup too_long(std::wstring(300, L'x'));
    THROWS_AS(too_long.set_enabled(true), std::invalid_argument);
}
