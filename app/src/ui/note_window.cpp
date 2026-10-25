#include "todo/note_view.hpp"
#include "todo/ui.hpp"
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#ifdef TODO_USE_NIM
#include "todo/nim_surface.hpp"
#endif

namespace todo {
std::string NoteView::motion_state() const {
#ifdef TODO_USE_NIM
    return nim_->motion_state();
#else
    throw std::runtime_error("Motion inspection requires the nim_duilib frontend");
#endif
}
namespace {
constexpr wchar_t note_class[] = L"DesktopTodo.Note";
constexpr wchar_t unlock_class[] = L"DesktopTodo.Unlock";
}
NoteView::NoteView(Application& app,Id id) : app_(app),id_(id) {
#ifdef TODO_USE_NIM
    nim_=std::make_unique<NimSurface>(*this);
#endif
    WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.hInstance=app.instance();
    cls.lpfnWndProc=proc;cls.lpszClassName=note_class;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    cls.style=CS_DBLCLKS;
    cls.hIcon=LoadIconW(app.instance(),MAKEINTRESOURCEW(101));
    RegisterClassExW(&cls);
    cls.lpfnWndProc=unlock_proc;cls.lpszClassName=unlock_class;
    RegisterClassExW(&cls);
}
NoteView::~NoteView() {
    destroy();
    for (auto font : {text_font_,title_font_,done_font_,icon_font_}) if (font) DeleteObject(font);
}
void NoteView::destroy() {
    destroying_=true;
    if (IsWindow(unlock_)) DestroyWindow(unlock_);
    if (IsWindow(window_)) DestroyWindow(window_);
    window_=unlock_=editor_=nullptr;
}
Layout NoteView::layout() const {
    RECT rect{};GetClientRect(window_,&rect);
    Layout result{rect.right,rect.bottom,dpi_,scroll_};
#ifdef TODO_USE_NIM
    result.toolbar_gap=12;
#endif
    if (const auto* current=note()) result.scroll=result.clamped_scroll(visible_tasks(*current).size());
    return result;
}
void NoteView::fonts() {
    for (auto font : {text_font_,title_font_,done_font_,icon_font_}) if (font) DeleteObject(font);
    text_font_=ui::font(dpi_,10);title_font_=ui::font(dpi_,12,FW_SEMIBOLD);
    done_font_=ui::font(dpi_,10,FW_NORMAL,true);icon_font_=ui::font(dpi_,12,FW_NORMAL,false,L"Segoe UI Emoji");
    if (editor_) SendMessageW(editor_,WM_SETFONT,reinterpret_cast<WPARAM>(text_font_),TRUE);
}
void NoteView::synchronize() {
    const auto* current=note();if (!current) return;
    applying_=true;
    struct Reset { bool& value;~Reset(){value=false;} } reset{applying_};
    if (!IsWindow(window_)) {
        destroying_=false;applied_.reset();
#ifdef TODO_USE_NIM
        window_=nim_->create();
#else
        window_=CreateWindowExW(WS_EX_TOOLWINDOW,note_class,win::wide(current->title).c_str(),
            WS_POPUP|WS_CLIPCHILDREN,0,0,340,380,nullptr,nullptr,app_.instance(),this);
#endif
        if (!window_) throw std::runtime_error(win::last_error("Create note"));
        dpi_=int(GetDpiForWindow(window_));fonts();
    }
    if (!IsWindow(unlock_)) {
        unlock_=CreateWindowExW(WS_EX_TOOLWINDOW,unlock_class,L"解锁便签",WS_POPUP,0,0,26,26,
            nullptr,nullptr,app_.instance(),this);
        if (!unlock_) throw std::runtime_error(win::last_error("Create unlock control"));
    }
    SetWindowTextW(window_,win::wide(current->title).c_str());
#ifdef TODO_USE_NIM
    nim_->synchronize(*current);
#endif
    HWND host=current->mode==Mode::desktop ? win::desktop_host() : nullptr;
    bool host_changed=current->mode==Mode::desktop && (!IsWindow(host)||GetParent(window_)!=host);
    bool style_changed=!applied_ || applied_->mode!=current->mode || applied_->locked!=current->locked || host_changed;
    bool bounds_changed=!applied_ || applied_->bounds!=current->bounds;
    if (style_changed) win::apply_mode(window_,*current,host);
    else if (bounds_changed && !moving_ && !resizing_)
        win::set_screen_bounds(window_,current->bounds,current->mode==Mode::desktop ? GetParent(window_) : nullptr);
    if(applied_&&applied_->mode!=current->mode)hidden_=false;
    if (!IsWindowVisible(window_)&&!hidden_) ui::show(window_);
    applied_=Applied{current->mode,current->locked,current->bounds,current->corner,current->lock_offset};
    int dpi=int(GetDpiForWindow(window_));if(dpi && dpi!=dpi_) {dpi_=dpi;fonts();}
    update_scroll();sync_editor();sync_unlock();invalidate();
}
void NoteView::show() {
    if (!window_) return;
    hidden_=false;ui::show(window_);
    if (const auto* current=note();current && current->mode!=Mode::desktop) SetForegroundWindow(window_);
    sync_unlock();
}
void NoteView::invalidate() {
    if (window_) InvalidateRect(window_,nullptr,FALSE);
    if (unlock_) InvalidateRect(unlock_,nullptr,FALSE);
}
void NoteView::sync_unlock() {
    if (const auto* current=note();current && IsWindow(unlock_) && IsWindow(window_))
        win::configure_unlock(unlock_,window_,*current);
}
void NoteView::update_scroll() {
    const auto* current=note();if (!current || !window_) return;
    auto area=layout();auto count=visible_tasks(*current).size();scroll_=area.clamped_scroll(count);
#ifdef TODO_USE_NIM
    ShowScrollBar(window_,SB_VERT,FALSE);return; // nim_duilib owns its client scrollbars.
#endif
    // UpdateLayeredWindow sizes the entire desktop HWND. A native non-client
    // scrollbar makes its client bitmap narrower and shrinks it on each paint.
    // Desktop cards scroll internally and paint their own scrollbar instead.
    if (current->mode==Mode::desktop) { ShowScrollBar(window_,SB_VERT,FALSE);return; }
    SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
    info.nMin=0;info.nMax=std::max(0,int(count)-1);info.nPage=UINT(area.page_rows());info.nPos=scroll_;
    SetScrollInfo(window_,SB_VERT,&info,TRUE);
}
void NoteView::menu(POINT point) { app_.note_menu(id_,point); }

LRESULT CALLBACK NoteView::proc(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<NoteView*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if (message==WM_NCCREATE) {
        self=static_cast<NoteView*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->window_=window;
    }
    if (!self) return DefWindowProcW(window,message,w,l);
    try {
        if (message==WM_NCDESTROY) {
            self->window_=nullptr;self->editor_=nullptr;self->applied_.reset();
            if (!self->destroying_) self->app_.shell_changed();
            SetWindowLongPtrW(window,GWLP_USERDATA,0);
            return DefWindowProcW(window,message,w,l);
        }
        return self->message(message,w,l);
    } catch (const std::exception& error) { self->app_.error(error);return 0; }
}
LRESULT CALLBACK NoteView::unlock_proc(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<NoteView*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if (message==WM_NCCREATE) {
        self=static_cast<NoteView*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->unlock_=window;
    }
    if (!self) return DefWindowProcW(window,message,w,l);
    try {
        if (message==WM_LBUTTONUP) {
            self->app_.change([&](Model& m){auto& note=note_by_id(m,self->id_);set_lock(m,self->id_,!note.locked);});return 0;
        }
        if (message==WM_PAINT) { self->paint_unlock();return 0; }
        if (message==WM_ERASEBKGND) return 1;
        if (message==WM_SETCURSOR) {SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}
        if (message==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
        if (message==WM_NCDESTROY) {self->unlock_=nullptr;SetWindowLongPtrW(window,GWLP_USERDATA,0);}
    } catch (const std::exception& error) { self->app_.error(error);return 0; }
    return DefWindowProcW(window,message,w,l);
}

LRESULT NoteView::message(UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case WM_PAINT:paint();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:update_scroll();sync_editor();sync_unlock();invalidate();return 0;
    case WM_WINDOWPOSCHANGED:
        if (!applying_) {sync_unlock();}
        break;
    case WM_DPICHANGED: {
        dpi_=HIWORD(w);fonts();auto* rect=reinterpret_cast<RECT*>(l);
        Bounds bounds{rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,dpi_};
        win::set_screen_bounds(window_,bounds,GetWindowLongPtrW(window_,GWL_STYLE)&WS_CHILD ? GetParent(window_) : nullptr);
        if (!applying_) end_move();return 0;
    }
    case WM_LBUTTONDOWN:mouse_down(GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;
    case WM_MOUSEMOVE:mouse_move(GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;
    case WM_LBUTTONUP:mouse_up(GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;
    case WM_LBUTTONDBLCLK: {
        auto* current=note();if(!current)return 0;
        auto hit=layout().hit(*current,GET_X_LPARAM(l),GET_Y_LPARAM(l));
        if(hit.kind==HitKind::title)begin_edit(0,true,false);
        else if(hit.kind==HitKind::text)begin_edit(hit.task,false,false);
        return 0;
    }
    case WM_CAPTURECHANGED:if (moving_||resizing_||drag_task_)cancel_drag();return 0;
    case WM_EXITSIZEMOVE:end_move();return 0;
    case WM_MOUSEWHEEL:scroll_-=GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*3;update_scroll();sync_editor();invalidate();return 0;
    case message_readonly_wheel:
        scroll_-=int(w)/WHEEL_DELTA*3;update_scroll();invalidate();return 0;
    case WM_VSCROLL: {
        SCROLLINFO info{};info.cbSize=sizeof(info);info.fMask=SIF_ALL;GetScrollInfo(window_,SB_VERT,&info);
        switch(LOWORD(w)) {
        case SB_LINEUP:--scroll_;break;case SB_LINEDOWN:++scroll_;break;
        case SB_PAGEUP:scroll_-=layout().page_rows();break;case SB_PAGEDOWN:scroll_+=layout().page_rows();break;
        case SB_THUMBTRACK:scroll_=info.nTrackPos;break;default:break;
        } update_scroll();sync_editor();invalidate();return 0;
    }
    case WM_CONTEXTMENU: {
        POINT point{GET_X_LPARAM(l),GET_Y_LPARAM(l)};
        if(point.x==-1&&point.y==-1){point={40,layout().list_top()};ClientToScreen(window_,&point);}
        POINT local=point;ScreenToClient(window_,&local);auto* current=note();
        auto hit=current ? layout().hit(*current,local.x,local.y) : Hit{};
        if(hit.task)task_menu(hit.task,point);else menu(point);return 0;
    }
    case WM_COMMAND:
        if(reinterpret_cast<HWND>(l)==editor_)invalidate();
        if(reinterpret_cast<HWND>(l)==editor_ && HIWORD(w)==EN_KILLFOCUS && !editing_new_ && !cancelling_edit_ && !destroying_)
            finish_edit(true,false);
        return 0;
    case WM_KEYDOWN:
        if(w==VK_ESCAPE){cancel_drag();return 0;}
        if(GetKeyState(VK_CONTROL)&0x8000) {
            if(w=='Z')app_.undo();else if(w=='Y')app_.redo();else if(w=='N')app_.new_note();return 0;
        }
        if(w==VK_F2 && selected_)begin_edit(selected_,false,false);
        if(w==VK_DELETE && selected_){Id task=selected_;selected_=0;app_.change([&](Model& m){remove_task(m,id_,task);});}
        if(w==VK_RETURN)begin_edit(0,false,true);
        return 0;
    case WM_SETCURSOR: {
        if(LOWORD(l)!=HTCLIENT)break;
        POINT point{};GetCursorPos(&point);ScreenToClient(window_,&point);auto* current=note();
        auto hit=current ? layout().hit(*current,point.x,point.y) : Hit{};
        auto cursor=hit.kind==HitKind::resize?IDC_SIZENWSE:hit.kind==HitKind::grip?IDC_SIZEALL:
            (hit.kind==HitKind::menu||hit.kind==HitKind::filter||hit.kind==HitKind::mode||hit.kind==HitKind::lock||hit.kind==HitKind::status||hit.kind==HitKind::icon)?IDC_HAND:IDC_ARROW;
        SetCursor(LoadCursorW(nullptr,cursor));return TRUE;
    }
    case WM_CLOSE:hidden_=true;ui::hide(window_);ui::hide(unlock_);app_.show_manager();return 0;
    }
    return DefWindowProcW(window_,message,w,l);
}
}
