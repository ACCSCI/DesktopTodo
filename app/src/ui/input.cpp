#include "todo/note_view.hpp"
#include "todo/ui.hpp"
#include <algorithm>
#include <cstdlib>

namespace todo {
void NoteView::mouse_down(int x,int y) {
    const auto* current=note();if(!current)return;
    auto hit=layout().hit(*current,x,y);
    if(hit.task)selected_=hit.task;
    POINT screen{x,y};ClientToScreen(window_,&screen);
    switch(hit.kind) {
    case HitKind::menu:menu(screen);break;
    case HitKind::filter:filter_menu(screen);break;
    case HitKind::mode:mode_menu(screen);break;
    case HitKind::lock:app_.change([&](Model& m){set_lock(m,id_,true);});break;
    case HitKind::status: {
        auto status=task_by_id(*current,hit.task).status==Status::done?Status::pending:Status::done;
        app_.change([&](Model& m){set_status(m,id_,hit.task,status,ui::now());});break;
    }
    case HitKind::icon:task_menu(hit.task,screen);break;
    case HitKind::add:
        if(layout().add_rect().contains(x,y)&&editing_new_)finish_edit(true);
        else begin_edit(0,false,true);
        break;
    case HitKind::title:
    case HitKind::resize:
        if(editor_&&!editing_new_)finish_edit(true);
        anchor_=screen;drag_bounds_=win::screen_bounds(window_);
        moving_=hit.kind==HitKind::title;resizing_=hit.kind==HitKind::resize;
        SetCapture(window_);break;
    case HitKind::grip:
        if(editor_&&!editing_new_)finish_edit(true);
        drag_task_=hit.task;anchor_=screen;dragging_=false;SetCapture(window_);break;
    case HitKind::text:SetFocus(window_);invalidate();break;
    default:break;
    }
}
void NoteView::mouse_move(int x,int y) {
    POINT screen{x,y};ClientToScreen(window_,&screen);
    if(moving_||resizing_) {
        Bounds bounds=drag_bounds_;
        if(moving_){bounds.x+=screen.x-anchor_.x;bounds.y+=screen.y-anchor_.y;}
        else {
            bounds.width=std::clamp(bounds.width+int(screen.x-anchor_.x),pixels(240,dpi_),8000);
            bounds.height=std::clamp(bounds.height+int(screen.y-anchor_.y),pixels(180,dpi_),8000);
        }
        const auto* current=note();if(!current)return;
        win::set_screen_bounds(window_,bounds,current->mode==Mode::desktop?GetParent(window_):nullptr);
        sync_unlock();return;
    }
    if(drag_task_) {
        if(std::abs(screen.x-anchor_.x)+std::abs(screen.y-anchor_.y)>pixels(4,dpi_))dragging_=true;
        if(dragging_) {
            const auto* current=note();if(!current)return;
            drop_before_=layout().drop_before(*current,y);
            if(y<layout().list_top()+pixels(10,dpi_))--scroll_;
            else if(y>layout().list_bottom()-pixels(10,dpi_))++scroll_;
            update_scroll();invalidate();
        }
    }
}
void NoteView::mouse_up(int x,int y) {
    const bool moved=moving_||resizing_, sorted=dragging_;
    const Id task=drag_task_, before=drop_before_;
    moving_=resizing_=dragging_=false;drag_task_=drop_before_=0;
    if(GetCapture()==window_)ReleaseCapture();
    if(moved){end_move();return;}
    if(sorted&&task) {
        POINT screen{x,y};ClientToScreen(window_,&screen);
        NoteView* target=app_.view_at(screen);
        if(target&&target!=this) {
            auto* target_note=target->note();if(!target_note||window_spec(*target_note).click_through)return;
            POINT point=screen;ScreenToClient(target->window(),&point);
            Id target_id=target->id();Id destination=target->layout().drop_before(*target_note,point.y);
            app_.change([&](Model& m){move_task(m,id_,target_id,task,destination);});
        } else if(layout().hit(*note(),x,y).kind!=HitKind::none || (y>=layout().list_top()&&y<=layout().list_bottom())) {
            app_.change([&](Model& m){reorder_visible(m,id_,task,before);});
        }
    }
    invalidate();
}
void NoteView::end_move() {
    if(applying_||destroying_||!IsWindow(window_)||!note())return;
    auto bounds=win::visible_bounds(win::screen_bounds(window_));
    app_.change([&](Model& m){set_bounds(m,id_,bounds);},false);
    sync_unlock();
}
void NoteView::cancel_drag() {
    const bool restore=moving_||resizing_;
    moving_=resizing_=dragging_=false;drag_task_=drop_before_=0;
    if(GetCapture()==window_)ReleaseCapture();
    if(restore&&note())win::set_screen_bounds(window_,drag_bounds_,note()->mode==Mode::desktop?GetParent(window_):nullptr);
    sync_unlock();invalidate();
}
}
