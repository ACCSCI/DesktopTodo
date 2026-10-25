#pragma once
#include "windows.hpp"
#include <optional>

namespace todo::ui {
HFONT font(int dpi,int size,int weight = FW_NORMAL,bool strike = false,const wchar_t* family = L"Segoe UI");
void fill(HDC dc,RECT rect,COLORREF color);
void text(HDC dc,RECT rect,const std::wstring& value,HFONT font,COLORREF color,UINT flags = DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
COLORREF blend(COLORREF a,COLORREF b,int weight);
COLORREF foreground(COLORREF background);
std::wstring filter_name(Filter filter);
std::wstring mode_name(Mode mode);
std::optional<std::wstring> prompt(HWND owner,const std::wstring& title,const std::wstring& initial,int limit);
std::optional<Color> choose_color(HWND owner,Color initial);
std::int64_t now();
void show(HWND window);
void hide(HWND window);
}
