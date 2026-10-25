#include "test.hpp"
#include "todo/windows.hpp"
using namespace todo;

TEST_CASE("the application and tray icon is embedded as a real Windows icon resource") {
    auto icon=LoadIconW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101));
    CHECK(icon!=nullptr);ICONINFO info{};CHECK(GetIconInfo(icon,&info));
    CHECK(info.hbmColor!=nullptr);
    BITMAP bitmap{};CHECK(GetObjectW(info.hbmColor,sizeof(bitmap),&bitmap));
    CHECK(bitmap.bmWidth>=16&&bitmap.bmHeight>=16);
    if(info.hbmColor)DeleteObject(info.hbmColor);if(info.hbmMask)DeleteObject(info.hbmMask);
}

TEST_CASE("vector control icons draw visible pixels inside their supplied bounds at multiple scales") {
    HDC screen=GetDC(nullptr);HDC dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,128,128);ReleaseDC(nullptr,screen);
    auto old=SelectObject(dc,bitmap);
    struct Cleanup {HDC dc;HBITMAP bitmap;HGDIOBJ old;~Cleanup(){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}} cleanup{dc,bitmap,old};
    for(int size:{16,24,48})for(auto glyph:{win::Glyph::lock,win::Glyph::unlock,win::Glyph::more,win::Glyph::plus,win::Glyph::grip,win::Glyph::chevron,win::Glyph::check,win::Glyph::clock,win::Glyph::note}) {
        RECT whole{0,0,128,128};FillRect(dc,&whole,reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        Rect icon{16,16,16+size,16+size};win::draw_glyph(dc,icon,glyph,RGB(50,60,90));
        int changed=0;
        for(int y=0;y<128;++y)for(int x=0;x<128;++x)if(GetPixel(dc,x,y)!=RGB(255,255,255)) {
            CHECK(icon.contains(x,y));++changed;
        }
        CHECK(changed>4);
    }
}

TEST_CASE("the closed lock has a connected shackle and the open lock has a clear gap") {
    HDC screen=GetDC(nullptr);HDC dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,48,48);ReleaseDC(nullptr,screen);
    auto old=SelectObject(dc,bitmap);
    struct Cleanup {HDC dc;HBITMAP bitmap;HGDIOBJ old;~Cleanup(){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}} cleanup{dc,bitmap,old};
    RECT whole{0,0,48,48};
    for(auto glyph:{win::Glyph::lock,win::Glyph::unlock}) {
        FillRect(dc,&whole,reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        win::draw_glyph(dc,{0,0,48,48},glyph,RGB(0,0,0));
        CHECK((GetPixel(dc,34,16)!=RGB(255,255,255))==(glyph==win::Glyph::lock));
        CHECK(GetPixel(dc,24,30)!=RGB(255,255,255));
    }
}
