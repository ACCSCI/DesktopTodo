#include "todo/nim_surface.hpp"
#include "todo/note_view.hpp"
#include "todo/ui.hpp"
#include "duilib/duilib.h"
#include <windowsx.h>
#include <algorithm>
#include <optional>
#include "row_motion.hpp"

namespace todo {
namespace dui=::ui;
struct NimSurface::Impl : dui::WindowImplBase {
    NoteView& owner;
    dui::RichEdit2 *input=nullptr,*active=nullptr,*title=nullptr;
    dui::VListBox* list=nullptr;
    std::optional<Note> previous;
    bool syncing=false;
    RowMotion motion;
    int drag_scroll=0;
    std::string last_hit;
    explicit Impl(NoteView& value):owner(value) {}
    DString GetSkinFolder() override {return L"todo";}
    DString GetSkinFile() override {return L"note.xml";}
    void OnFinalMessage() override {} // NoteView owns this adapter.
    HWND hwnd() const {return static_cast<HWND>(GetWindowHandle());}
    template<class T> T* control(const wchar_t* name) {return dynamic_cast<T*>(FindControl(name));}
    void safe(const std::function<void()>& action) {
        try {action();}catch(const std::exception& error){owner.app_.error(error);}
    }
    POINT screen() {POINT point{};::GetCursorPos(&point);return point;}
    void commit(bool save,bool focus) {
        if(!active)return;
        auto* field=active;
        std::string text=win::utf8(field->GetText());
        bool is_title=owner.editing_title_,is_new=owner.editing_new_;
        Id task=owner.edit_task_;
        if(!is_new) {active=nullptr;field->SetReadOnly(true);}
        if(save&&!text.empty()) safe([&] {
            owner.app_.change([&](Model& model) {
                if(is_title)rename_note(model,owner.id_,text);
                else if(is_new) {
                    owner.selected_=add_task(model,owner.id_,text,ui::now());
                    if(!matches(note_by_id(model,owner.id_).filter,Status::pending))set_filter(model,owner.id_,Filter::all);
                }
                else edit_task(model,owner.id_,task,text);
            });
            if(is_new)field->SetText(L"");
        });
        if(!is_new) {owner.editing_new_=true;owner.editing_title_=false;owner.edit_task_=0;active=input;}
        if(focus&&active)active->SetFocus();
    }
    void edit(dui::RichEdit2* field,Id task,bool is_title,bool is_new) {
        if(!field||window_spec(*owner.note()).click_through)return;
        active=field;owner.edit_task_=task;owner.editing_title_=is_title;owner.editing_new_=is_new;
        field->SetReadOnly(false);field->SetEnableWheelZoom(false);field->SetFocus();field->SetSelAll();
    }
    void OnInitWindow() override {
        input=control<dui::RichEdit2>(L"input");title=control<dui::RichEdit2>(L"title");
        list=control<dui::VListBox>(L"tasks");
        if(!input||!title||!list)throw std::runtime_error("Incomplete nim_duilib note layout");
        active=input;
        input->SetEnableWheelZoom(false);title->SetEnableWheelZoom(false);
        input->AttachSetFocus([this](const dui::EventArgs&){edit(input,0,false,true);return true;});
        input->AttachReturn([this](const dui::EventArgs&){commit(true,false);return true;});
        title->AttachDoubleClick([this](const dui::EventArgs&){edit(title,0,true,false);return true;});
        title->AttachReturn([this](const dui::EventArgs&){commit(true,true);return true;});
        title->AttachKillFocus([this](const dui::EventArgs&){if(active==title&&!syncing)commit(true,false);return true;});
        control<dui::Button>(L"add")->AttachClick([this](const dui::EventArgs&){active=input;owner.editing_new_=true;owner.editing_title_=false;commit(true,true);return true;});
        control<dui::Button>(L"menu")->AttachClick([this](const dui::EventArgs&){owner.menu(screen());return true;});
        control<dui::Button>(L"filter")->AttachClick([this](const dui::EventArgs&){owner.filter_menu(screen());return true;});
        control<dui::Button>(L"mode")->AttachClick([this](const dui::EventArgs&){owner.mode_menu(screen());return true;});
        list->AttachScrollPosChanged([this](const dui::EventArgs&){owner.scroll_=int(list->GetScrollPos().cy)/pixels(38,owner.dpi_);return true;});
    }
    void rows(const Note& note) {
        if(active&&active!=input&&active!=title)active=input;
        motion.rebuild(pixels(38,owner.dpi_));
        list->RemoveAllItems();
        for(auto id:visible_tasks(note)) {
            const auto& task=task_by_id(note,id);
            auto* row=new dui::ListBoxItem(this);
            row->SetAttribute(L"height",L"38");row->SetAttribute(L"padding",L"4,3,4,3");
            auto* content=new dui::HBox(this);content->SetAttribute(L"child_margin",L"2");
            auto* grip=new dui::Button(this);grip->SetAttribute(L"width",L"20");
            grip->SetAttribute(L"bkimage",L"file='move.svg' width='14' height='14' halign='center' valign='center'");
            grip->AttachButtonDown([this,id](const dui::EventArgs& event){
                ::SetFocus(hwnd());
                owner.drag_task_=id;owner.anchor_={event.ptMouse.x,event.ptMouse.y};
                ::ClientToScreen(hwnd(),&owner.anchor_);drag_scroll=int(list->GetScrollPos().cy);
                owner.dragging_=false;SetCapture();return false;
            });
            auto* check=new dui::CheckBox(this);check->SetClass(L"checkbox_1");check->SetAttribute(L"width",L"20");check->Selected(task.status==Status::done);
            check->SetAttribute(L"normal_image",task.status==Status::doing?L"file='doing.svg' width='18' height='18' halign='center' valign='center'":L"file='pending.svg' width='18' height='18' halign='center' valign='center'");
            check->SetAttribute(L"selected_normal_image",L"file='done.svg' width='18' height='18' halign='center' valign='center'");
            check->AttachClick([this,id](const dui::EventArgs&){safe([&]{owner.app_.change([&](Model& m){auto state=task_by_id(note_by_id(m,owner.id_),id).status;set_status(m,owner.id_,id,state==Status::done?Status::pending:Status::done,ui::now());});});return true;});
            auto* icon=new dui::Button(this);icon->SetAttribute(L"width",L"20");icon->SetText(win::wide(task.icon));icon->SetAttribute(L"text_align",L"hcenter,vcenter");
            if(task.icon=="•") {icon->SetText(L"");icon->SetAttribute(L"bkimage",L"file='dot.svg' width='18' height='18' halign='center' valign='center'");}
            icon->AttachClick([this,id](const dui::EventArgs&){owner.task_menu(id,screen());return true;});
            auto* text=new dui::RichEdit2(this);text->SetReadOnly(true);text->SetEnableWheelZoom(false);text->SetText(win::wide(task.text));
            for(dui::Control* item:std::initializer_list<dui::Control*>{grip,check,icon,text}) {
                item->SetAttribute(L"height",L"24");item->SetAttribute(L"valign",L"center");
            }
            text->SetAttribute(L"font",L"system_regular_14");
            text->SetAttribute(L"margin",L"4,0,0,0");
            text->SetAttribute(L"text_align",L"vcenter");
            wchar_t task_color[12]{};swprintf_s(task_color,L"#FF%06X",task.color);
            if(task.icon=="•")icon->SetAttribute(L"bkimage",std::wstring(L"file='dot.svg' width='18' height='18' halign='center' valign='center' svg_replace_colors='#64748b|")+task_color+L"'");
            text->SetAttribute(L"normal_text_color",task.status==Status::done?L"#FF94A3B8":task_color);
            text->AttachDoubleClick([this,id,text](const dui::EventArgs&){edit(text,id,false,false);return true;});
            text->AttachReturn([this](const dui::EventArgs&){commit(true,true);return true;});
            text->AttachKillFocus([this,text](const dui::EventArgs&){if(active==text&&!syncing)commit(true,false);return true;});
            row->AttachRClick([this,id](const dui::EventArgs&){owner.selected_=id;owner.task_menu(id,screen());return true;});
            content->AddItem(grip);content->AddItem(check);content->AddItem(icon);content->AddItem(text);row->AddItem(content);list->AddItem(row);
            motion.add(id,row);
        }
    }
    void sync(const Note& note) {
        syncing=true;
        SetLayeredWindow(note.mode==Mode::desktop,false);
        if(active!=title)title->SetText(win::wide(note.title));
        control<dui::Button>(L"filter")->SetText(ui::filter_name(note.filter));
        control<dui::Button>(L"mode")->SetText(ui::mode_name(note.mode));
        control<dui::Control>(L"lock_slot")->SetVisible(note.mode==Mode::desktop);
        control<dui::Label>(L"count")->SetText(std::to_wstring(note.tasks.size())+L" 项");
        COLORREF background=ui::blend(RGB(255,255,255),win::color(note.color),72);
        unsigned rgb=(GetRValue(background)<<16)|(GetGValue(background)<<8)|GetBValue(background);
        wchar_t color[12]{};swprintf_s(color,L"#FF%06X",rgb);
        control<dui::Control>(L"surface")->SetBkColor(color);
        if(!previous||previous->tasks!=note.tasks||previous->filter!=note.filter)rows(note);
        bool enabled=!window_spec(note).click_through;
        if(!enabled&&owner.drag_task_) {owner.cancel_drag();motion.settle();}
        list->SetEnabled(enabled);input->SetEnabled(enabled);control<dui::Button>(L"add")->SetEnabled(enabled);
        previous=note;syncing=false;
    }
    LRESULT OnWindowMessage(UINT message,WPARAM w,LPARAM l,bool& handled) override {
        if(message==message_readonly_wheel) {
            auto position=list->GetScrollPos();position.cy-=int(w)*pixels(38,owner.dpi_)*3/WHEEL_DELTA;
            list->SetScrollPos(position);handled=true;return 0;
        }
        if(message==WM_LBUTTONDOWN) {
            auto* hit=FindControl(dui::UiPoint(GET_X_LPARAM(l),GET_Y_LPARAM(l)));
            last_hit=hit?win::utf8(hit->GetType()):"none";
        }
        if(message==WM_CLOSE) {handled=true;return owner.message(message,w,l);}
        if(message==WM_WINDOWPOSCHANGED&&owner.window_)owner.sync_unlock();
        if(message==WM_EXITSIZEMOVE)safe([&]{owner.end_move();});
        if((message==WM_MOUSEMOVE||message==WM_LBUTTONUP)&&owner.drag_task_) {
            handled=true;
            if(message==WM_MOUSEMOVE) {
                owner.mouse_move(GET_X_LPARAM(l),GET_Y_LPARAM(l));
                auto scroll=list->GetScrollPos();scroll.cy=owner.scroll_*pixels(38,owner.dpi_);list->SetScrollPos(scroll);
                POINT pointer{GET_X_LPARAM(l),GET_Y_LPARAM(l)};::ClientToScreen(hwnd(),&pointer);
                if(owner.dragging_)motion.preview(owner.drag_task_,owner.drop_before_,int(pointer.y-owner.anchor_.y)+int(list->GetScrollPos().cy)-drag_scroll);
            }
            else {motion.capture();safe([&]{owner.mouse_up(GET_X_LPARAM(l),GET_Y_LPARAM(l));});motion.settle();}
            return 0;
        }
        if(message==WM_CAPTURECHANGED&&owner.drag_task_) {owner.cancel_drag();motion.settle();}
        if(message==WM_KEYDOWN&&w==VK_ESCAPE&&owner.drag_task_) {
            owner.cancel_drag();motion.settle();handled=true;return 0;
        }
        if(message==WM_KEYDOWN&&(GetKeyState(VK_CONTROL)&0x8000)&&(w=='Z'||w=='Y'||w=='N')) {
            handled=true;return owner.message(message,w,l);
        }
        return WindowImplBase::OnWindowMessage(message,w,l,handled);
    }
};
NimSurface::NimSurface(NoteView& owner):impl_(std::make_unique<Impl>(owner)) {}
NimSurface::~NimSurface()=default;
HWND NimSurface::create() {
    impl_->SetRenderBackendType(dui::RenderBackendType::kRaster_BackendType);
    impl_->SetShadowAttached(false);
    if(!impl_->CreateWnd(nullptr,dui::WindowCreateParam(L"DesktopTodo",false)))throw std::runtime_error("Create nim_duilib note");
    return impl_->hwnd();
}
void NimSurface::synchronize(const Note& note) {impl_->sync(note);}
void NimSurface::begin_edit(Id task,bool title,bool add) {
    if(add)impl_->edit(impl_->input,0,false,true);
    else if(title)impl_->edit(impl_->title,0,true,false);
    else { // Double-click handlers select the specific row; menu edits use the existing prompt.
        auto value=ui::prompt(impl_->hwnd(),L"编辑待办",win::wide(task_by_id(*impl_->owner.note(),task).text),1000);
        if(value)impl_->owner.app_.change([&](Model& m){edit_task(m,impl_->owner.id_,task,win::utf8(*value));});
    }
}
void NimSurface::finish_edit(bool commit,bool focus) {impl_->commit(commit,focus);}
void NimSurface::set_text(const std::wstring& text) {if(impl_->active)impl_->active->SetText(text);}
bool NimSurface::focused() const {return impl_->active&&impl_->active->IsFocused();}
std::string NimSurface::motion_state() const {
    auto rect=[](dui::Control* control) {
        auto r=control->GetPos();return "["+std::to_string(r.left)+","+std::to_string(r.top)+","+std::to_string(r.right)+","+std::to_string(r.bottom)+"]";
    };
    return "{\"scroll\":"+std::to_string(impl_->list->GetScrollPos().cy)+",\"footer\":["+rect(impl_->input)+","+rect(impl_->control<dui::Button>(L"add"))+"]"+
        ",\"dragging\":"+std::string(impl_->owner.dragging_?"true":"false")+",\"dragTask\":"+std::to_string(impl_->owner.drag_task_)+",\"hit\":\""+impl_->last_hit+"\",\"rows\":"+impl_->motion.inspect()+"}";
}
}

