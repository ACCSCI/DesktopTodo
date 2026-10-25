#include "todo/note_view.hpp"
#include "todo/ui.hpp"
#include <algorithm>

namespace todo {
namespace {
RECT rect(Rect value) {return {value.left,value.top,value.right,value.bottom};}
void rounded(HDC dc,RECT area,COLORREF color,int radius) {
    auto brush=CreateSolidBrush(color);auto old_b=SelectObject(dc,brush);
    auto old_p=SelectObject(dc,GetStockObject(NULL_PEN));
    RoundRect(dc,area.left,area.top,area.right,area.bottom,radius,radius);
    SelectObject(dc,old_p);SelectObject(dc,old_b);DeleteObject(brush);
}
}
void NoteView::paint() {
    PAINTSTRUCT paint{};HDC target=BeginPaint(window_,&paint);
    const auto* current=note();if(!current){EndPaint(window_,&paint);return;}
    RECT client{};GetClientRect(window_,&client);
    HDC dc=CreateCompatibleDC(target);
    HBITMAP bitmap=CreateCompatibleBitmap(target,std::max(1L,client.right),std::max(1L,client.bottom));
    auto old_bitmap=SelectObject(dc,bitmap);
    auto p=[&](int dip){return pixels(dip,dpi_);};
    auto area=layout();
    COLORREF tint=win::color(current->color),background=ui::blend(RGB(255,255,255),tint,72);
    COLORREF foreground=ui::foreground(background),accent=RGB(105,89,205),muted=ui::blend(foreground,background,55);
    ui::fill(dc,client,background);
    ui::fill(dc,{0,0,client.right,area.header()},RGB(233,229,250));
    ui::fill(dc,{0,area.header()-p(1),client.right,area.header()},RGB(210,202,233));
    win::draw_glyph(dc,{p(10),p(15),p(22),p(29)},win::Glyph::grip,RGB(117,101,153));
    ui::text(dc,{p(29),0,client.right-p(current->mode==Mode::desktop?78:46),area.header()},win::wide(current->title),title_font_,foreground);
    auto menu=area.menu_rect(*current),filter=area.filter_rect(),mode=area.mode_rect();
    rounded(dc,rect(menu),ui::blend(RGB(255,255,255),tint,70),p(10));
    win::draw_glyph(dc,{menu.left+p(5),menu.top+p(5),menu.right-p(5),menu.bottom-p(5)},win::Glyph::more,muted);
    rounded(dc,rect(filter),ui::blend(RGB(255,255,255),tint,40),p(12));
    ui::text(dc,{filter.left+p(10),filter.top,filter.right-p(20),filter.bottom},ui::filter_name(current->filter),text_font_,foreground);
    int toolbar_center=(filter.top+filter.bottom)/2;
    win::draw_glyph(dc,{filter.right-p(18),toolbar_center-p(6),filter.right-p(6),toolbar_center+p(6)},win::Glyph::chevron,muted);
    ui::text(dc,{mode.left+p(4),mode.top,mode.right-p(22),mode.bottom},ui::mode_name(current->mode),text_font_,muted);
    win::draw_glyph(dc,{mode.right-p(18),toolbar_center-p(6),mode.right-p(6),toolbar_center+p(6)},win::Glyph::chevron,muted);
    auto visible=visible_tasks(*current);
    int completed=0;for(const auto& task:current->tasks)if(task.status==Status::done)++completed;
    ui::text(dc,{client.right-p(75),area.header(),client.right-p(14),area.list_top()},
        std::to_wstring(completed)+L" / "+std::to_wstring(current->tasks.size()),text_font_,muted,DT_RIGHT|DT_SINGLELINE|DT_VCENTER);
    int saved=SaveDC(dc);IntersectClipRect(dc,0,area.list_top(),client.right,area.list_bottom());
    if(visible.empty()) {
        int center=client.right/2;
        win::draw_glyph(dc,{center-p(18),area.list_top()+p(36),center+p(18),area.list_top()+p(72)},win::Glyph::note,ui::blend(accent,background,45));
        ui::text(dc,{p(20),area.list_top()+p(88),client.right-p(20),area.list_bottom()},
            current->tasks.empty()?L"给今天留一点计划\n在下方写下第一件待办":L"这里暂时没有待办",text_font_,muted,DT_CENTER|DT_WORDBREAK);
    }
    for(int index=area.scroll;index<int(visible.size())&&index<=area.scroll+area.page_rows();++index) {
        const auto& task=task_by_id(*current,visible[std::size_t(index)]);
        RECT row=rect(area.task_rect(index));row.top+=p(2);row.bottom-=p(2);
        RECT shadow=row;shadow.top+=p(1);shadow.bottom+=p(1);
        rounded(dc,shadow,ui::blend(foreground,background,8),p(12));
        rounded(dc,row,task.id==selected_?RGB(240,237,255):RGB(255,255,255),p(12));
        int cy=(row.top+row.bottom)/2;
        win::draw_glyph(dc,{p(11),cy-p(7),p(21),cy+p(7)},win::Glyph::grip,RGB(178,181,194));
        COLORREF status=task.status==Status::done?RGB(22,139,96):task.status==Status::doing?RGB(217,119,6):RGB(148,163,184);
        HPEN pen=CreatePen(PS_SOLID,p(2),status);auto old_pen=SelectObject(dc,pen);
        HBRUSH brush=CreateSolidBrush(task.status==Status::done?status:RGB(255,255,255));auto old_brush=SelectObject(dc,brush);
        if(task.status==Status::doing)win::draw_glyph(dc,{p(28),cy-p(8),p(44),cy+p(8)},win::Glyph::clock,status);
        else Ellipse(dc,p(29),cy-p(7),p(43),cy+p(7));
        SelectObject(dc,old_brush);DeleteObject(brush);SelectObject(dc,old_pen);DeleteObject(pen);
        if(task.status==Status::done)win::draw_glyph(dc,{p(30),cy-p(6),p(42),cy+p(6)},win::Glyph::check,RGB(255,255,255));
        if(task.icon=="•") {
            HBRUSH dot=CreateSolidBrush(win::color(task.color));auto brush=SelectObject(dc,dot);
            auto pen=SelectObject(dc,GetStockObject(NULL_PEN));int r=p(2);
            Ellipse(dc,p(61)-r,cy-r,p(61)+r+1,cy+r+1);
            SelectObject(dc,pen);SelectObject(dc,brush);DeleteObject(dot);
        } else ui::text(dc,{p(49),row.top,p(73),row.bottom},win::wide(task.icon),icon_font_,win::color(task.color),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        if(!(editor_&&!editing_new_&&!editing_title_&&edit_task_==task.id))
            ui::text(dc,{p(78),row.top,client.right-p(18),row.bottom},win::wide(task.text),
                task.status==Status::done?done_font_:text_font_,task.status==Status::done?ui::blend(win::color(task.color),RGB(255,255,255),48):win::color(task.color));
        if(dragging_&&drop_before_==task.id)ui::fill(dc,{p(10),row.top,client.right-p(10),row.top+p(2)},RGB(37,99,235));
    }
    if(dragging_&&!drop_before_&&!visible.empty()) {
        auto last=area.task_rect(int(visible.size())-1);ui::fill(dc,{p(10),last.bottom-p(2),client.right-p(10),last.bottom},RGB(37,99,235));
    }
    RestoreDC(dc,saved);
    ui::fill(dc,{p(12),area.list_bottom(),client.right-p(12),area.list_bottom()+p(1)},ui::blend(foreground,background,9));
    if(current->mode==Mode::desktop&&current->locked)
        ui::text(dc,{p(16),area.list_bottom(),client.right-p(14),client.bottom},L"已锁定 · 点击右上角锁形按钮解锁",text_font_,muted);
    else {
        auto input=area.input_rect(),add=area.add_rect();
        rounded(dc,rect(input),RGB(255,255,255),p(12));
        if(editor_&&editing_new_&&GetFocus()==editor_) {
            HPEN focus=CreatePen(PS_SOLID,std::max(1,p(1)),accent);auto pen=SelectObject(dc,focus);
            auto brush=SelectObject(dc,GetStockObject(NULL_BRUSH));
            RoundRect(dc,input.left,input.top,input.right,input.bottom,p(12),p(12));
            SelectObject(dc,brush);SelectObject(dc,pen);DeleteObject(focus);
        }
        rounded(dc,rect(add),accent,p(10));
        win::draw_glyph(dc,{add.left+p(5),add.top+p(5),add.right-p(5),add.bottom-p(5)},win::Glyph::plus,RGB(255,255,255));
    }
    HPEN grip=CreatePen(PS_SOLID,1,ui::blend(foreground,background,30));auto previous=SelectObject(dc,grip);
    for(int i=3;i<=9;i+=3){MoveToEx(dc,client.right-p(i),client.bottom-p(2),nullptr);LineTo(dc,client.right-p(2),client.bottom-p(i));}
    SelectObject(dc,previous);DeleteObject(grip);
    if(current->mode==Mode::desktop&&int(visible.size())>area.page_rows()) {
        int track=std::max(1,area.list_bottom()-area.list_top());
        int thumb=std::max(p(24),track*area.page_rows()/int(visible.size()));
        int max_scroll=std::max(1,int(visible.size())-area.page_rows());
        int y=area.list_top()+(track-thumb)*area.scroll/max_scroll;
        rounded(dc,{client.right-p(5),y,client.right-p(2),y+thumb},ui::blend(accent,background,55),p(2));
    }
    // Desktop cards are explicitly composed; include the native edit control
    // in the supplied pixels rather than relying on Explorer's GDI redirection.
    if(current->mode==Mode::desktop&&editor_&&IsWindowVisible(editor_)) {
        RECT edit{};GetWindowRect(editor_,&edit);
        POINT position{edit.left,edit.top};ScreenToClient(window_,&position);
        int saved_edit=SaveDC(dc);SetViewportOrgEx(dc,position.x,position.y,nullptr);
        SendMessageW(editor_,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);
        if(GetFocus()==editor_) {
            DWORD start=0,end=0;SendMessageW(editor_,EM_GETSEL,reinterpret_cast<WPARAM>(&start),reinterpret_cast<LPARAM>(&end));
            POINT caret{};
            if(start==end&&GetCaretPos(&caret)) {
                RECT bounds{};GetClientRect(editor_,&bounds);IntersectClipRect(dc,0,0,bounds.right,bounds.bottom);
                ui::fill(dc,{caret.x,caret.y,caret.x+std::max(1,p(1)),std::min(bounds.bottom,caret.y+p(16))},accent);
            }
        }
        RestoreDC(dc,saved_edit);
    }
    std::exception_ptr failure;
    try {
        if(GetWindowLongPtrW(window_,GWL_EXSTYLE)&WS_EX_LAYERED)
            win::present_layered(window_,dc,client.right,client.bottom);
        else BitBlt(target,0,0,client.right,client.bottom,dc,0,0,SRCCOPY);
    } catch(...) {failure=std::current_exception();}
    SelectObject(dc,old_bitmap);DeleteObject(bitmap);DeleteDC(dc);EndPaint(window_,&paint);
    if(failure)std::rethrow_exception(failure);
}
void NoteView::paint_unlock() {
    PAINTSTRUCT paint{};HDC target=BeginPaint(unlock_,&paint);RECT area{};GetClientRect(unlock_,&area);
    HDC dc=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,std::max(1L,area.right),std::max(1L,area.bottom));
    auto old=SelectObject(dc,bitmap);
    const auto* current=note();bool locked=current&&current->locked;
    COLORREF tint=current?win::color(current->color):RGB(255,244,194);
    ui::fill(dc,area,ui::blend(RGB(255,255,255),tint,40));
    rounded(dc,{0,0,area.right,area.bottom},locked?RGB(105,89,205):RGB(239,236,253),pixels(9,dpi_));
    int inset=pixels(5,dpi_);
    win::draw_glyph(dc,{inset,inset,area.right-inset,area.bottom-inset},locked?win::Glyph::lock:win::Glyph::unlock,locked?RGB(255,255,255):RGB(105,89,205));
    std::exception_ptr failure;
    try {win::present_layered(unlock_,dc,area.right,area.bottom);}catch(...){failure=std::current_exception();}
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);EndPaint(unlock_,&paint);
    if(failure)std::rethrow_exception(failure);
}
}
