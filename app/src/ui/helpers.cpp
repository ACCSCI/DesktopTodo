#include "todo/ui.hpp"
#include <commdlg.h>
#include <chrono>

namespace todo::ui {
HFONT font(int dpi,int size,int weight,bool strike,const wchar_t* family) {
    return CreateFontW(-MulDiv(size,dpi,72),0,0,0,weight,FALSE,strike,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,family);
}
void fill(HDC dc,RECT rect,COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc,&rect,brush);
    DeleteObject(brush);
}
void text(HDC dc,RECT rect,const std::wstring& value,HFONT face,COLORREF color,UINT flags) {
    auto old = SelectObject(dc,face);
    SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT);
    DrawTextW(dc,value.c_str(),int(value.size()),&rect,flags);
    SelectObject(dc,old);
}
COLORREF blend(COLORREF a,COLORREF b,int weight) {
    auto part = [&](int x,int y) { return (x*weight+y*(100-weight))/100; };
    return RGB(part(GetRValue(a),GetRValue(b)),part(GetGValue(a),GetGValue(b)),part(GetBValue(a),GetBValue(b)));
}
COLORREF foreground(COLORREF background) {
    int light = GetRValue(background)*299+GetGValue(background)*587+GetBValue(background)*114;
    return light < 125000 ? RGB(248,250,252) : RGB(30,41,59);
}
std::wstring filter_name(Filter filter) {
    switch (filter) {
    case Filter::all: return L"全部";
    case Filter::unfinished: return L"未完成";
    case Filter::pending: return L"待开始";
    case Filter::doing: return L"进行中";
    case Filter::done: return L"已完成";
    }
    return L"全部";
}
std::wstring mode_name(Mode mode) {
    switch(mode) { case Mode::desktop:return L"桌面";case Mode::topmost:return L"置顶";case Mode::window:return L"窗口"; }
    return L"桌面";
}
std::optional<Color> choose_color(HWND owner,Color initial) {
    static COLORREF custom[16]{};
    CHOOSECOLORW color{};
    color.lStructSize = sizeof(color); color.hwndOwner = owner;
    color.rgbResult = win::color(initial); color.lpCustColors = custom;
    color.Flags = CC_FULLOPEN|CC_RGBINIT;
    if (!ChooseColorW(&color)) return {};
    return win::color_value(color.rgbResult);
}
std::int64_t now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
void show(HWND window) {
    SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
}
void hide(HWND window) {
    SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_HIDEWINDOW);
}
}
