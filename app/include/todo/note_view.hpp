#pragma once
#include "application.hpp"
#include <optional>

namespace todo {
class NimSurface;
class NoteView {
    friend class NimSurface;
#ifdef TODO_USE_NIM
    std::unique_ptr<NimSurface> nim_;
#endif
    Application& app_;
    Id id_;
    HWND window_ = nullptr, unlock_ = nullptr, editor_ = nullptr;
    HFONT text_font_ = nullptr, title_font_ = nullptr, done_font_ = nullptr, icon_font_ = nullptr;
    int dpi_ = 96, scroll_ = 0;
    Id selected_ = 0, edit_task_ = 0, drag_task_ = 0, drop_before_ = 0;
    bool editing_title_ = false, editing_new_ = false, cancelling_edit_ = false;
    bool destroying_ = false, applying_ = false, moving_ = false, resizing_ = false, dragging_ = false;
    bool hidden_ = false;
    POINT anchor_{};
    Bounds drag_bounds_;
    struct Applied { Mode mode; bool locked; Bounds bounds; Corner corner; int offset; };
    std::optional<Applied> applied_;
    std::wstring draft_;
    void fonts();
    void paint();
    void paint_unlock();
    void update_scroll();
    void sync_editor();
    void sync_unlock();
    void mouse_down(int x,int y);
    void mouse_move(int x,int y);
    void mouse_up(int x,int y);
    void end_move();
    void cancel_drag();
    void begin_edit(Id task, bool title, bool add);
    void finish_edit(bool commit, bool focus = true);
    void task_menu(Id task, POINT point);
    void filter_menu(POINT point);
    void mode_menu(POINT point);
    LRESULT message(UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK unlock_proc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK edit_proc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
public:
    NoteView(Application& app,Id id);
    ~NoteView();
    void synchronize();
    void destroy();
    HWND window() const { return window_; }
    HWND unlock_window() const { return unlock_; }
    HWND editor_window() const { return editor_; }
    void focus_editor() { begin_edit(0,false,true); }
    void set_editor_text(const std::wstring& text);
    void commit_editor() { finish_edit(true); }
    bool editor_focused() const;
    std::string motion_state() const;
    Id id() const { return id_; }
    const Note* note() const { return app_.find_note(id_); }
    Layout layout() const;
    void show();
    void menu(POINT point);
    void invalidate();
};
}
