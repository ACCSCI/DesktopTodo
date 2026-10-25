#pragma once
#include "model.hpp"

namespace todo {
struct Rect {
    int left = 0, top = 0, right = 0, bottom = 0;
    int width() const { return right - left; }
    int height() const { return bottom - top; }
    bool contains(int x, int y) const { return x >= left && x < right && y >= top && y < bottom; }
    bool operator==(const Rect&) const = default;
};
struct WindowSpec {
    bool desktop_child = false;
    bool topmost = false;
    bool taskbar = false;
    bool click_through = false;
    bool separate_unlock = false;
    bool activate_on_show = false;
};
WindowSpec window_spec(const Note& note);
inline bool accept_locked_wheel(const Note& note,bool pointer_inside,bool desktop_exposed) {
    return note.mode==Mode::desktop&&note.locked&&pointer_inside&&desktop_exposed;
}
int pixels(int dip, int dpi);
Rect unlock_rect(const Note& note, int client_width, int client_height, int dpi);
Bounds fit_bounds(Bounds bounds, Rect work_area, int target_dpi);

enum class HitKind { none, title, menu, lock, filter, mode, add, grip, status, icon, text, resize };
struct Hit { HitKind kind = HitKind::none; Id task = 0; };
struct Layout {
    int width, height, dpi, scroll = 0;
    int toolbar_gap = 0;
    int header() const { return pixels(44, dpi); }
    int list_top() const { return pixels(78+toolbar_gap, dpi); }
    int list_bottom() const { return height - pixels(44, dpi); }
    int row_height() const { return pixels(38, dpi); }
    int page_rows() const;
    int clamped_scroll(std::size_t count) const;
    Rect task_rect(int visible_index) const;
    Rect menu_rect(const Note& note) const;
    Rect filter_rect() const;
    Rect mode_rect() const;
    Rect input_rect() const;
    Rect add_rect() const;
    Hit hit(const Note& note, int x, int y) const;
    Id drop_before(const Note& note, int y) const;
};
}
