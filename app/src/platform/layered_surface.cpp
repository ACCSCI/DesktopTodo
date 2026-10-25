#include "todo/windows.hpp"
#include <stdexcept>

namespace todo::win {
void present_layered(HWND window,HDC source,int width,int height) {
    if(!is_owned(window))throw std::invalid_argument("Foreign layered window");
    if(width<=0||height<=0)return;
    if(width>8000||height>8000)throw std::invalid_argument("Layered surface is too large");
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* bytes=nullptr;
    HBITMAP bitmap=CreateDIBSection(source,&info,DIB_RGB_COLORS,&bytes,nullptr,0);
    HDC memory=CreateCompatibleDC(source);
    if(!bitmap||!memory) {
        if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);
        throw std::runtime_error(last_error("Allocate desktop surface"));
    }
    auto old=SelectObject(memory,bitmap);
    bool copied=BitBlt(memory,0,0,width,height,source,0,0,SRCCOPY)!=FALSE;
    GdiFlush();
    // GDI writes RGB but does not preserve alpha. Every card pixel is opaque.
    auto* pixels=static_cast<std::uint32_t*>(bytes);
    for(std::size_t i=0;i<std::size_t(width)*height;++i)pixels[i]|=0xFF000000;
    POINT origin{};SIZE size{width,height};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
    bool submitted=copied&&UpdateLayeredWindow(window,nullptr,nullptr,&size,memory,&origin,0,&blend,ULW_ALPHA);
    DWORD error=GetLastError();
    SelectObject(memory,old);DeleteDC(memory);DeleteObject(bitmap);
    if(!submitted)throw std::runtime_error(last_error("Present desktop surface",error));
    auto count=reinterpret_cast<std::uintptr_t>(GetPropW(window,L"DesktopTodo.Presentations"));
    SetPropW(window,L"DesktopTodo.Presentations",reinterpret_cast<HANDLE>(count+1));
}
}
