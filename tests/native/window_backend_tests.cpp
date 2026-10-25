#include "test.hpp"
#include "todo/windows.hpp"
using namespace todo;

TEST_CASE("desktop reattachment preserves locked policy and physical bounds across hosts") {
    HWND first=CreateWindowExW(0,L"STATIC",L"first host",WS_POPUP,0,0,800,600,nullptr,nullptr,nullptr,nullptr);
    HWND second=CreateWindowExW(0,L"STATIC",L"replacement host",WS_POPUP,40,30,800,600,nullptr,nullptr,nullptr,nullptr);
    HWND body=CreateWindowExW(0,L"STATIC",L"note",WS_POPUP,0,0,1,1,nullptr,nullptr,nullptr,nullptr);
    struct Cleanup { HWND a,b,c;~Cleanup(){DestroyWindow(c);DestroyWindow(b);DestroyWindow(a);} } cleanup{first,second,body};
    CHECK(first&&second&&body);
    Note note;note.locked=true;note.bounds={130,140,340,380,96};
    win::apply_mode(body,note,first);
    win::apply_mode(body,note,second);
    CHECK(GetParent(body)==second);
    CHECK(GetWindowLongPtrW(body,GWL_EXSTYLE)&WS_EX_TRANSPARENT);
    auto bounds=win::screen_bounds(body);
    CHECK(bounds.x==130&&bounds.y==140&&bounds.width==340&&bounds.height==380);
    CHECK(!IsWindowVisible(body));
}

struct HiddenWindows {
    HWND host = CreateWindowExW(0,L"STATIC",L"DesktopTodo headless parent",WS_POPUP,0,0,800,600,nullptr,nullptr,nullptr,nullptr);
    HWND body = CreateWindowExW(0,L"STATIC",L"DesktopTodo headless body",WS_POPUP,10,10,340,380,nullptr,nullptr,nullptr,nullptr);
    HiddenWindows() { if (!host || !body) throw std::runtime_error("Cannot create hidden test windows"); }
    ~HiddenWindows() { if (IsWindow(body)) DestroyWindow(body); if (IsWindow(host)) DestroyWindow(host); }
};

TEST_CASE("desktop selection uses the content surface when the root has no redirected bitmap") {
    HiddenWindows windows;
    HWND content=CreateWindowExW(0,L"STATIC",L"desktop content",WS_CHILD,0,0,800,600,windows.host,nullptr,nullptr,nullptr);
    CHECK(content);
    CHECK(win::desktop_surface(windows.host,content)==windows.host);
    HWND modern=CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP,L"STATIC",L"composed root",WS_POPUP,0,0,800,600,nullptr,nullptr,nullptr,nullptr);
    HWND composed=CreateWindowExW(0,L"STATIC",L"composed content",WS_CHILD,0,0,800,600,modern,nullptr,nullptr,nullptr);
    struct Cleanup {HWND window;~Cleanup(){DestroyWindow(window);}} cleanup{modern};
    CHECK(modern&&composed);
    CHECK(GetWindowLongPtrW(modern,GWL_EXSTYLE)&WS_EX_NOREDIRECTIONBITMAP);
    CHECK(win::desktop_surface(modern,composed)==composed);
    CHECK(win::desktop_surface(windows.host,windows.body)==nullptr);
    CHECK(win::desktop_surface(nullptr,content)==nullptr);
    DestroyWindow(content);
}

TEST_CASE("native window adapter changes modes on hidden windows without an Explorer dependency") {
    HiddenWindows windows;
    Note note;
    auto apply = [&](const char* stage, HWND parent) {
        try { win::apply_mode(windows.body,note,parent); }
        catch (const std::exception& error) { throw std::runtime_error(std::string(stage) + ": " + error.what()); }
    };
    note.locked = true;
    apply("desktop locked",windows.host);
    CHECK(GetParent(windows.body) == windows.host);
    CHECK(GetWindowLongPtrW(windows.body,GWL_STYLE) & WS_CHILD);
    CHECK(GetWindowLongPtrW(windows.body,GWL_EXSTYLE) & WS_EX_TRANSPARENT);
    CHECK(!IsWindowVisible(windows.body));
    note.locked = false;
    apply("desktop unlocked",windows.host);
    CHECK(!(GetWindowLongPtrW(windows.body,GWL_EXSTYLE) & WS_EX_TRANSPARENT));
    CHECK(GetWindowLongPtrW(windows.body,GWL_EXSTYLE) & WS_EX_LAYERED);
    note.mode = Mode::topmost;
    apply("topmost",nullptr);
    CHECK(GetParent(windows.body) == nullptr);
    CHECK(GetWindowLongPtrW(windows.body,GWL_EXSTYLE) & WS_EX_TOPMOST);
    note.mode = Mode::window;
    apply("ordinary",nullptr);
    CHECK(!(GetWindowLongPtrW(windows.body,GWL_EXSTYLE) & WS_EX_TOPMOST));
    CHECK(GetWindowLongPtrW(windows.body,GWL_EXSTYLE) & WS_EX_APPWINDOW);
    CHECK(GetWindowLongPtrW(windows.body,GWL_STYLE) & WS_CAPTION);
}

TEST_CASE("missing desktop hosts and foreign windows fail before changing native state") {
    HiddenWindows windows;
    Note note;
    auto previous = GetWindowLongPtrW(windows.body,GWL_STYLE);
    THROWS_AS(win::apply_mode(windows.body,note,nullptr), std::runtime_error);
    CHECK(GetParent(windows.body) == nullptr);
    CHECK(GetWindowLongPtrW(windows.body,GWL_STYLE) == previous);
    THROWS_AS(win::apply_mode(GetDesktopWindow(),note,windows.host), std::invalid_argument);
}

TEST_CASE("locked child surfaces submit opaque GDI pixels without redirected layered painting") {
    HiddenWindows windows;Note note;note.locked=true;
    win::apply_mode(windows.body,note,windows.host);
    HDC source=CreateCompatibleDC(nullptr);
    HDC screen=GetDC(nullptr);HBITMAP bitmap=CreateCompatibleBitmap(screen,32,32);ReleaseDC(nullptr,screen);
    auto old=SelectObject(source,bitmap);
    struct Cleanup {HDC dc;HBITMAP bitmap;HGDIOBJ old;~Cleanup(){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}} cleanup{source,bitmap,old};
    RECT area{0,0,32,32};HBRUSH brush=CreateSolidBrush(RGB(255,244,194));
    FillRect(source,&area,brush);DeleteObject(brush);
    CHECK(GetPixel(source,10,10)==RGB(255,244,194));
    win::present_layered(windows.body,source,32,32);
    CHECK(GetWindowLongPtrW(windows.body,GWL_EXSTYLE)&WS_EX_TRANSPARENT);
    win::present_layered(windows.body,source,32,32);
    note.locked=false;win::apply_mode(windows.body,note,windows.host);
    win::present_layered(windows.body,source,32,32);
    CHECK(!(GetWindowLongPtrW(windows.body,GWL_EXSTYLE)&WS_EX_TRANSPARENT));
    THROWS_AS(win::present_layered(GetDesktopWindow(),source,32,32),std::invalid_argument);
}
