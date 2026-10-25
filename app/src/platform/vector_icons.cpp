#include "todo/windows.hpp"
#include <algorithm>

namespace todo::win {
void draw_glyph(HDC dc,Rect area,Glyph glyph,COLORREF color) {
    const int size=std::min(area.width(),area.height());if(size<8)return;
    const int left=area.left+(area.width()-size)/2,top=area.top+(area.height()-size)/2;
    auto x=[&](int n){return left+MulDiv(n,size,24);};
    auto y=[&](int n){return top+MulDiv(n,size,24);};
    LOGBRUSH stroke{BS_SOLID,color,0};
    HPEN pen=ExtCreatePen(PS_GEOMETRIC|PS_SOLID|PS_ENDCAP_ROUND|PS_JOIN_ROUND,std::max(1,size/12),&stroke,0,nullptr);
    auto old_pen=SelectObject(dc,pen);auto old_brush=SelectObject(dc,GetStockObject(NULL_BRUSH));
    auto line=[&](int a,int b,int c,int d){MoveToEx(dc,x(a),y(b),nullptr);LineTo(dc,x(c),y(d));};
    auto dot=[&](int a,int b){HBRUSH brush=CreateSolidBrush(color);SelectObject(dc,brush);Ellipse(dc,x(a)-std::max(1,size/20),y(b)-std::max(1,size/20),x(a)+std::max(1,size/20)+1,y(b)+std::max(1,size/20)+1);SelectObject(dc,GetStockObject(NULL_BRUSH));DeleteObject(brush);};
    switch(glyph) {
    case Glyph::lock:case Glyph::unlock:
        {
            const bool closed=glyph==Glyph::lock;
            int a=closed?7:9,b=closed?17:19;
            MoveToEx(dc,x(a),y(11),nullptr);LineTo(dc,x(a),y(7));
            POINT curve[]={{x(a),y(1)},{x(b),y(1)},{x(b),y(7)}};
            PolyBezierTo(dc,curve,3);
            if(closed)LineTo(dc,x(b),y(11));
            RoundRect(dc,x(4),y(10),x(20),y(22),std::max(2,size/5),std::max(2,size/5));
            line(12,14,12,18);
        }break;
    case Glyph::more:dot(5,12);dot(12,12);dot(19,12);break;
    case Glyph::plus:line(12,5,12,19);line(5,12,19,12);break;
    case Glyph::grip:for(int row:{6,12,18}){dot(8,row);dot(16,row);}break;
    case Glyph::chevron:line(7,10,12,15);line(12,15,17,10);break;
    case Glyph::check:line(5,12,10,17);line(10,17,19,7);break;
    case Glyph::clock:Ellipse(dc,x(3),y(3),x(21),y(21));line(12,7,12,12);line(12,12,16,14);break;
    case Glyph::note:RoundRect(dc,x(4),y(3),x(20),y(21),std::max(2,size/5),std::max(2,size/5));line(8,9,16,9);line(8,13,16,13);line(8,17,13,17);break;
    }
    SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(pen);
}
}
