#include "todo/note_view.hpp"
#include "todo/ui.hpp"
#include <commctrl.h>
#include <imm.h>
#include <algorithm>

namespace todo {
void NoteView::set_editor_text(const std::wstring& text) { if(editor_)SetWindowTextW(editor_,text.c_str()); }
bool NoteView::editor_focused() const { return editor_ && GetFocus()==editor_; }
namespace {
std::wstring contents(HWND edit) {
    int count=GetWindowTextLengthW(edit);std::wstring text(std::size_t(count)+1,L'\0');
    GetWindowTextW(edit,text.data(),count+1);text.resize(count);return text;
}
}
void NoteView::sync_editor() {
    const auto* current=note();if(!current||!window_)return;
    if(window_spec(*current).click_through) {
        if(editor_){cancelling_edit_=true;DestroyWindow(editor_);editor_=nullptr;cancelling_edit_=false;}
        return;
    }
    if(!editor_) {
        editing_new_=true;editing_title_=false;edit_task_=0;
        editor_=CreateWindowExW(0,L"EDIT",draft_.c_str(),
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,0,0,1,1,window_,reinterpret_cast<HMENU>(200),app_.instance(),nullptr);
        if(!editor_)throw std::runtime_error(win::last_error("Task editor"));
        SetWindowSubclass(editor_,edit_proc,1,reinterpret_cast<DWORD_PTR>(this));
        SendMessageW(editor_,WM_SETFONT,reinterpret_cast<WPARAM>(text_font_),TRUE);
        SendMessageW(editor_,EM_SETLIMITTEXT,2048,0);
        SendMessageW(editor_,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);
        SendMessageW(editor_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(L"添加待办，按 Enter"));
    }
    auto area=layout();auto input=area.input_rect();
    HFONT face=editing_title_?title_font_:text_font_;
    if(reinterpret_cast<HFONT>(SendMessageW(editor_,WM_GETFONT,0,0))!=face)
        SendMessageW(editor_,WM_SETFONT,reinterpret_cast<WPARAM>(face),TRUE);
    HDC dc=GetDC(window_);auto old=SelectObject(dc,face);TEXTMETRICW metrics{};
    GetTextMetricsW(dc,&metrics);SelectObject(dc,old);ReleaseDC(window_,dc);
    int height=std::max(pixels(16,dpi_),int(metrics.tmHeight)+2);
    int x=input.left+pixels(8,dpi_),y=input.top+(input.height()-height)/2;
    int width=input.width()-pixels(16,dpi_);
    if(editing_title_) {
        x=pixels(29,dpi_);y=(area.header()-height)/2;
        width=area.width-pixels(current->mode==Mode::desktop?78:46,dpi_)-x;
    }
    else if(!editing_new_) {
        auto visible=visible_tasks(*current);auto it=std::find(visible.begin(),visible.end(),edit_task_);
        if(it==visible.end()){finish_edit(false);return;}
        auto row=area.task_rect(int(it-visible.begin()));x=pixels(78,dpi_);y=row.top+(row.height()-height)/2;
        width=area.width-x-pixels(18,dpi_);
    }
    SetWindowPos(editor_,HWND_TOP,x,y,std::max(20,width),std::max(16,height),SWP_NOACTIVATE|SWP_SHOWWINDOW);
}
void NoteView::begin_edit(Id task,bool title,bool add) {
    const auto* current=note();if(!current||window_spec(*current).click_through)return;
    if(editor_&&editing_new_)draft_=contents(editor_);
    if(editor_&&!editing_new_)finish_edit(true);
    current=note();if(!current)return;
    sync_editor();
    editing_new_=add;editing_title_=title;edit_task_=task;
    std::wstring text=add?draft_:title?win::wide(current->title):win::wide(task_by_id(*current,task).text);
    SetWindowTextW(editor_,text.c_str());
    SendMessageW(editor_,EM_SETLIMITTEXT,title?128:2048,0);
    SendMessageW(editor_,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(add?L"添加待办，按 Enter":L""));
    sync_editor();SetFocus(editor_);SendMessageW(editor_,EM_SETSEL,0,-1);invalidate();
}
void NoteView::finish_edit(bool commit,bool focus) {
    if(!editor_||cancelling_edit_)return;
    auto value=contents(editor_);
    const bool add=editing_new_,title=editing_title_;const Id task=edit_task_;
    if(add)draft_=value;
    if(commit) {
        if(value.find_first_not_of(L" \t\r\n")==std::wstring::npos) {
            if(add)return;
            throw std::invalid_argument("标题和待办内容不能为空");
        }
        auto text=win::utf8(value);Id created=0;
        app_.change([&](Model& model){
            if(add) {
                created=add_task(model,id_,text,ui::now());
                if(!matches(note_by_id(model,id_).filter,Status::pending))set_filter(model,id_,Filter::all);
            } else if(title)rename_note(model,id_,text);
            else edit_task(model,id_,task,text);
        });
        if(created)selected_=created;
        if(add)draft_.clear();
    }
    cancelling_edit_=true;DestroyWindow(editor_);editor_=nullptr;cancelling_edit_=false;
    editing_new_=true;editing_title_=false;edit_task_=0;
    sync_editor();
    if(commit&&focus)SetFocus(editor_);
    invalidate();
}
LRESULT CALLBACK NoteView::edit_proc(HWND edit,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data) {
    auto* self=reinterpret_cast<NoteView*>(data);
    try {
        if(message==WM_KEYDOWN&&(w==VK_RETURN||w==VK_ESCAPE)) {
            HIMC context=ImmGetContext(edit);
            bool composing=context&&ImmGetCompositionStringW(context,GCS_COMPSTR,nullptr,0)>0;
            if(context)ImmReleaseContext(edit,context);
            if(composing)return DefSubclassProc(edit,message,w,l);
            self->finish_edit(w==VK_RETURN);return 0;
        }
        if(message==WM_GETDLGCODE)return DLGC_WANTALLKEYS|DLGC_WANTCHARS;
        if(message==WM_NCDESTROY) {
            RemoveWindowSubclass(edit,edit_proc,id);
            if(self->editor_==edit)self->editor_=nullptr;
        }
    } catch(const std::exception& error){self->app_.error(error);return 0;}
    auto result=DefSubclassProc(edit,message,w,l);
    if(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||
       message==WM_KEYUP||message==WM_CHAR||message==WM_IME_COMPOSITION||
       (message==WM_MOUSEMOVE&&(w&MK_LBUTTON)))self->invalidate();
    return result;
}
}
