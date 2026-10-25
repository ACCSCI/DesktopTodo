// Native vector-icon contact sheet for visual review; no Explorer interaction.
#include "todo/windows.hpp"
#include <fstream>
#include <vector>

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    constexpr int width=320,height=160;
    HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* data=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);
    if(!dc||!bitmap)return 3;
    auto old=SelectObject(dc,bitmap);RECT area{0,0,width,height};
    FillRect(dc,&area,reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    todo::win::draw_glyph(dc,{24,24,120,120},todo::win::Glyph::lock,RGB(105,89,205));
    todo::win::draw_glyph(dc,{180,24,276,120},todo::win::Glyph::unlock,RGB(105,89,205));
    todo::win::draw_glyph(dc,{110,124,138,152},todo::win::Glyph::lock,RGB(105,89,205));
    todo::win::draw_glyph(dc,{160,124,188,152},todo::win::Glyph::unlock,RGB(105,89,205));
    GdiFlush();BITMAPFILEHEADER header{};header.bfType=0x4d42;
    header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
    header.bfSize=header.bfOffBits+width*height*4;
    std::ofstream file(argv[1],std::ios::binary);
    file.write(reinterpret_cast<const char*>(&header),sizeof(header));
    file.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));
    file.write(static_cast<const char*>(data),width*height*4);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    return file?0:4;
}
