#include "todo/windows.hpp"
#include <algorithm>
#include <stdexcept>

namespace todo::win {
namespace {
void style(HWND window, int index, LONG_PTR value) {
    SetLastError(0);
    LONG_PTR previous = SetWindowLongPtrW(window, index, value);
    DWORD error = GetLastError();
    if (!previous && error && GetWindowLongPtrW(window,index) != value)
        throw std::runtime_error(last_error(("Window style " + std::to_string(index)).c_str(),error));
}
void parent(HWND window, HWND target) {
    SetLastError(0);
    if (!SetParent(window, target) && GetLastError()) throw std::runtime_error(last_error("Desktop attachment"));
}
BOOL CALLBACK find_desktop(HWND window, LPARAM data) {
    HWND content=FindWindowExW(window,nullptr,L"SHELLDLL_DefView",nullptr);
    if (IsWindowVisible(window) && content) {
        *reinterpret_cast<HWND*>(data) = desktop_surface(window,content);
        return FALSE;
    }
    return TRUE;
}
}

bool is_owned(HWND window) {
    DWORD owner = 0;
    return IsWindow(window) && GetWindowThreadProcessId(window, &owner) && owner == GetCurrentProcessId();
}
HWND desktop_surface(HWND root,HWND content) {
    if(!IsWindow(root)||!IsWindow(content)||GetParent(content)!=root)return nullptr;
    // Modern Explorer's root may have no redirected bitmap. Put cards in its
    // actual desktop content surface, alongside the icon list, in that case.
    return (GetWindowLongPtrW(root,GWL_EXSTYLE)&WS_EX_NOREDIRECTIONBITMAP)?content:root;
}
HWND desktop_host() {
    HWND result = nullptr;
    EnumWindows(find_desktop, reinterpret_cast<LPARAM>(&result));
    return result;
}
Bounds screen_bounds(HWND window) {
    RECT rect{};
    if (!GetWindowRect(window, &rect)) throw std::runtime_error(last_error("Window bounds"));
    return {rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,int(GetDpiForWindow(window))};
}
void set_screen_bounds(HWND window, const Bounds& bounds, HWND target_parent) {
    if (!is_owned(window)) throw std::invalid_argument("Refusing to move a foreign window");
    POINT origin{bounds.x,bounds.y};
    if (target_parent) {
        SetLastError(0);
        if (!MapWindowPoints(nullptr,target_parent,&origin,1) && GetLastError())
            throw std::runtime_error(last_error("Map desktop position"));
    }
    if (!SetWindowPos(window,nullptr,origin.x,origin.y,bounds.width,bounds.height,
        SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED)) throw std::runtime_error(last_error("Set bounds"));
}
Bounds visible_bounds(Bounds bounds) {
    RECT rect{bounds.x,bounds.y,bounds.x+bounds.width,bounds.y+bounds.height};
    HMONITOR monitor = MonitorFromRect(&rect,MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor,&info)) throw std::runtime_error(last_error("Monitor work area"));
    UINT dpi_x = UINT(bounds.dpi), dpi_y = dpi_x;
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        using GetMonitorDpi = HRESULT(WINAPI*)(HMONITOR,int,UINT*,UINT*);
        auto get_dpi = reinterpret_cast<GetMonitorDpi>(GetProcAddress(shcore,"GetDpiForMonitor"));
        if (get_dpi) get_dpi(monitor,0,&dpi_x,&dpi_y);
        FreeLibrary(shcore);
    }
    const auto& work = info.rcWork;
    return fit_bounds(bounds,{work.left,work.top,work.right,work.bottom},int(dpi_x));
}

void apply_mode(HWND window, const Note& note, HWND desktop) {
    if (!is_owned(window)) throw std::invalid_argument("Refusing to reconfigure a foreign window");
    auto spec = window_spec(note);
    if (spec.desktop_child && !IsWindow(desktop)) throw std::runtime_error("Desktop host is not available");
    const HWND previous_parent = GetParent(window);
    const LONG_PTR previous_style = GetWindowLongPtrW(window,GWL_STYLE);
    const LONG_PTR previous_extended = GetWindowLongPtrW(window,GWL_EXSTYLE);
    const auto previous_bounds = screen_bounds(window);
    try {
        if (!SetWindowPos(window,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE))
            throw std::runtime_error(last_error("Change window order"));
        LONG_PTR desired = WS_CLIPCHILDREN|WS_CLIPSIBLINGS|(previous_style & WS_VISIBLE);
        LONG_PTR extended = previous_extended & ~(WS_EX_TOPMOST|WS_EX_APPWINDOW|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE);
        if (spec.desktop_child) {
            style(window,GWL_STYLE,desired|WS_CHILD);
            parent(window,desktop);
        } else {
            parent(window,nullptr);
            style(window,GWL_STYLE,desired|(spec.taskbar ? WS_OVERLAPPEDWINDOW : WS_POPUP));
        }
        extended |= spec.taskbar ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW;
        if (spec.desktop_child) extended |= WS_EX_LAYERED;
        if (spec.click_through) extended |= WS_EX_TRANSPARENT;
        style(window,GWL_EXSTYLE,extended);
        set_screen_bounds(window,note.bounds,spec.desktop_child ? desktop : nullptr);
        if (!SetWindowPos(window,spec.topmost ? HWND_TOPMOST : HWND_TOP,0,0,0,0,
            SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_FRAMECHANGED)) throw std::runtime_error(last_error("Set window layer"));
    } catch (...) {
        SetParent(window,previous_parent);
        SetWindowLongPtrW(window,GWL_STYLE,previous_style);
        SetWindowLongPtrW(window,GWL_EXSTYLE,previous_extended);
        try { set_screen_bounds(window,previous_bounds,(previous_style & WS_CHILD) ? previous_parent : nullptr); } catch (...) {}
        SetWindowPos(window,(previous_extended & WS_EX_TOPMOST) ? HWND_TOPMOST : HWND_NOTOPMOST,0,0,0,0,
            SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        throw;
    }
}

void configure_unlock(HWND button, HWND body, const Note& note) {
    if (!is_owned(button) || !is_owned(body)) throw std::invalid_argument("Foreign unlock window");
    auto spec = window_spec(note);
    SetWindowTextW(button,note.locked?L"解锁便签":L"锁定便签");
    if (!spec.separate_unlock || !IsWindowVisible(body)) {
        SetWindowPos(button,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_HIDEWINDOW);return;
    }
    HWND host = GetParent(body);
    if (!IsWindow(host)) throw std::runtime_error("Desktop parent disappeared");
    RECT client{};
    GetClientRect(body,&client);
    auto local = unlock_rect(note,client.right,client.bottom,int(GetDpiForWindow(body)));
    POINT point{local.left,local.top};
    MapWindowPoints(body,host,&point,1);
    style(button,GWL_STYLE,WS_CHILD|WS_CLIPSIBLINGS);
    style(button,GWL_EXSTYLE,WS_EX_TOOLWINDOW|WS_EX_LAYERED);
    parent(button,host);
    if (!SetWindowPos(button,HWND_TOP,point.x,point.y,local.width(),local.height(),
        SWP_NOACTIVATE|SWP_SHOWWINDOW)) throw std::runtime_error(last_error("Position unlock button"));
}
}
