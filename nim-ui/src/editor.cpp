#include "todo/note_view.hpp"
#include "todo/nim_surface.hpp"
namespace todo {
void NoteView::sync_editor() {const auto* current=note();editor_=current&&!window_spec(*current).click_through?window_:nullptr;}
void NoteView::begin_edit(Id task,bool title,bool add) {nim_->begin_edit(task,title,add);}
void NoteView::finish_edit(bool commit,bool focus) {nim_->finish_edit(commit,focus);}
void NoteView::set_editor_text(const std::wstring& text) {nim_->set_text(text);}
bool NoteView::editor_focused() const {return nim_->focused();}
LRESULT CALLBACK NoteView::edit_proc(HWND window,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR) {return DefWindowProcW(window,message,w,l);}
}
