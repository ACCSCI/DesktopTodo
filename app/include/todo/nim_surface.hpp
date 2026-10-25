#pragma once
#include "model.hpp"
#include <functional>
#include <memory>
#include <windows.h>
namespace todo {
class NoteView;
class NimSurface {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    explicit NimSurface(NoteView& owner);
    ~NimSurface();
    HWND create();
    void synchronize(const Note& note);
    void begin_edit(Id task, bool title, bool add);
    void finish_edit(bool commit, bool focus);
    void set_text(const std::wstring& text);
    bool focused() const;
    std::string motion_state() const;
};
int run_nim_ui(const std::function<int()>& run);
}
