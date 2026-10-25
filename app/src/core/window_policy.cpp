#include "todo/window_policy.hpp"
#include <algorithm>
#include <limits>

namespace todo {
WindowSpec window_spec(const Note& note) {
    const bool desktop = note.mode == Mode::desktop;
    return {desktop, note.mode == Mode::topmost, note.mode == Mode::window,
        desktop && note.locked, desktop, note.mode == Mode::window};
}
int pixels(int dip, int dpi) {
    return int((std::int64_t(dip) * dpi + (dip >= 0 ? 48 : -48)) / 96);
}
Rect unlock_rect(const Note& note, int width, int height, int dpi) {
    const int size = std::min({pixels(28, dpi), width, height});
    const int inset = std::max(0, std::min({pixels(note.lock_offset, dpi), (width - size) / 2, (height - size) / 2}));
    const bool right = note.corner == Corner::top_right || note.corner == Corner::bottom_right;
    const bool bottom = note.corner == Corner::bottom_left || note.corner == Corner::bottom_right;
    const int x = right ? width - size - inset : inset;
    const int vertical=std::min(std::max(inset,(pixels(44,dpi)-size)/2),std::max(0,(height-size)/2));
    const int y = bottom ? height - size - inset : vertical;
    return {x, y, x + size, y + size};
}
Bounds fit_bounds(Bounds bounds, Rect work, int target_dpi) {
    const int old_dpi = std::clamp(bounds.dpi, 48, 768);
    target_dpi = std::clamp(target_dpi, 48, 768);
    bounds.width = std::clamp(int(std::int64_t(bounds.width) * target_dpi / old_dpi),
        std::min(pixels(240, target_dpi), work.width()), work.width());
    bounds.height = std::clamp(int(std::int64_t(bounds.height) * target_dpi / old_dpi),
        std::min(pixels(180, target_dpi), work.height()), work.height());
    bounds.x = std::clamp(bounds.x, work.left, work.right - bounds.width);
    bounds.y = std::clamp(bounds.y, work.top, work.bottom - bounds.height);
    bounds.dpi = target_dpi;
    return bounds;
}
int Layout::page_rows() const { return std::max(1, (list_bottom() - list_top()) / row_height()); }
int Layout::clamped_scroll(std::size_t count) const {
    return std::clamp(scroll, 0, std::max(0, int(count) - page_rows()));
}
Rect Layout::task_rect(int index) const {
    int top = list_top() + (index - scroll) * row_height();
    return {pixels(8, dpi), top, width - pixels(8, dpi), top + row_height()};
}
Rect Layout::menu_rect(const Note& note) const {
    int right=width-pixels(note.mode==Mode::desktop?40:12,dpi);
    int top=(header()-pixels(28,dpi))/2;
    return {right-pixels(28,dpi),top,right,top+pixels(28,dpi)};
}
Rect Layout::filter_rect() const {
    int top=header()+(list_top()-header()-pixels(28,dpi))/2;
    return {pixels(12,dpi),top,pixels(92,dpi),top+pixels(28,dpi)};
}
Rect Layout::mode_rect() const {
    auto result=filter_rect();result.left=pixels(100,dpi);result.right=pixels(170,dpi);return result;
}
Rect Layout::input_rect() const {
    int top=list_bottom()+(height-list_bottom()-pixels(28,dpi))/2;
    return {pixels(12,dpi),top,width-pixels(48,dpi),top+pixels(28,dpi)};
}
Rect Layout::add_rect() const {
    auto result=input_rect();result.left=width-pixels(40,dpi);result.right=width-pixels(12,dpi);return result;
}
Hit Layout::hit(const Note& note, int x, int y) const {
    if (window_spec(note).click_through) return {};
    if (x < 0 || y < 0 || x >= width || y >= height) return {};
    if (x >= width - pixels(12, dpi) && y >= height - pixels(12, dpi)) return {HitKind::resize};
    if (y < header()) {
        if (note.mode == Mode::desktop) {
            if(unlock_rect(note,width,height,dpi).contains(x,y))return {HitKind::lock};
        }
        if(menu_rect(note).contains(x,y))return {HitKind::menu};
        return {HitKind::title};
    }
    if (y < list_top()) return {x < pixels(94, dpi) ? HitKind::filter : HitKind::mode};
    if (y >= list_bottom()) return {HitKind::add};
    auto visible = visible_tasks(note);
    int index = (y - list_top()) / row_height() + clamped_scroll(visible.size());
    if (index < 0 || std::size_t(index) >= visible.size()) return {};
    HitKind kind = x < pixels(24, dpi) ? HitKind::grip :
        x < pixels(48, dpi) ? HitKind::status : x < pixels(76, dpi) ? HitKind::icon : HitKind::text;
    return {kind, visible[std::size_t(index)]};
}
Id Layout::drop_before(const Note& note, int y) const {
    const auto visible = visible_tasks(note);
    const int index = std::max(0, (y - list_top() + row_height() / 2) / row_height()) + clamped_scroll(visible.size());
    return std::size_t(index) < visible.size() ? visible[std::size_t(index)] : 0;
}
}
