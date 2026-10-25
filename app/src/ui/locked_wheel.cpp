#include "todo/application.hpp"
#include "todo/note_view.hpp"
#include <windowsx.h>

namespace todo {
Application* Application::wheel_owner_=nullptr;
void Application::update_wheel_hook() {
    bool needed=false;
    if(!quitting_)for(const auto& [id,view]:views_) {
        auto* note=view->note();
        if(note&&note->mode==Mode::desktop&&note->locked&&IsWindowVisible(view->window()))needed=true;
    }
    if(needed&&!wheel_hook_) {
        wheel_owner_=this;
        wheel_hook_=SetWindowsHookExW(WH_MOUSE_LL,wheel_proc,instance_,0);
        if(!wheel_hook_) {wheel_owner_=nullptr;throw std::runtime_error(win::last_error("Read-only wheel hook"));}
    } else if(!needed&&wheel_hook_) {
        UnhookWindowsHookEx(wheel_hook_);wheel_hook_=nullptr;wheel_owner_=nullptr;
    }
}
LRESULT CALLBACK Application::wheel_proc(int code,WPARAM message,LPARAM data) {
    if(code==HC_ACTION&&message==WM_MOUSEWHEEL&&wheel_owner_) {
        const auto& event=*reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
        if(wheel_owner_->locked_wheel(event.pt,GET_WHEEL_DELTA_WPARAM(event.mouseData)))return 1;
    }
    return CallNextHookEx(nullptr,code,message,data);
}
bool Application::locked_wheel(POINT point,int delta,HWND hit) {
    if(quitting_)return false;
    if(!hit)hit=WindowFromPoint(point);
    NoteView* selected=nullptr;
    for(const auto& [id,view]:views_) {
        const auto* note=view->note();if(!note||!IsWindowVisible(view->window()))continue;
        RECT bounds{};if(!GetWindowRect(view->window(),&bounds))continue;
        HWND parent=GetParent(view->window());
        bool desktop=hit==parent||hit==view->window()||GetAncestor(hit,GA_ROOT)==GetAncestor(parent,GA_ROOT);
        // Other interactive note windows are independent wheel targets.
        for(const auto& [other_id,other]:views_)
            if(other.get()!=view.get()&&(hit==other->window()||hit==other->unlock_window()))desktop=false;
        if(!accept_locked_wheel(*note,PtInRect(&bounds,point)!=FALSE,desktop))continue;
        bool above=!selected;
        if(selected)for(HWND item=GetWindow(selected->window(),GW_HWNDPREV);item;item=GetWindow(item,GW_HWNDPREV))
            if(item==view->window()){above=true;break;}
        if(above)selected=view.get();
    }
    return selected&&PostMessageW(selected->window(),message_readonly_wheel,WPARAM(delta),0);
}
}
