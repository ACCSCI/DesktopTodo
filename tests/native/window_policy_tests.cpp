#include "test.hpp"
#include "todo/actions.hpp"
#include "todo/window_policy.hpp"
using namespace todo;

TEST_CASE("window modes map to desktop child, topmost and taskbar policies independently") {
    Note note;
    note.locked = true;
    auto desktop = window_spec(note);
    CHECK(desktop.desktop_child && desktop.click_through && desktop.separate_unlock);
    CHECK(!desktop.topmost && !desktop.taskbar && !desktop.activate_on_show);
    note.mode = Mode::topmost;
    auto topmost = window_spec(note);
    CHECK(topmost.topmost && !topmost.desktop_child && !topmost.click_through && !topmost.separate_unlock);
    note.mode = Mode::window;
    auto window = window_spec(note);
    CHECK(window.taskbar && window.activate_on_show && !window.topmost);
}

TEST_CASE("all four unlock corners remain within the note at multiple DPI scales") {
    Note note;
    for (int dpi : {96,144,192,240}) {
        for (auto corner : {Corner::top_left,Corner::top_right,Corner::bottom_left,Corner::bottom_right}) {
            note.corner = corner;
            auto rect = unlock_rect(note, pixels(340,dpi), pixels(380,dpi), dpi);
            CHECK(rect.width() == pixels(28,dpi));
            CHECK(rect.left >= 0 && rect.top >= 0);
            CHECK(rect.right <= pixels(340,dpi) && rect.bottom <= pixels(380,dpi));
        }
    }
    note.lock_offset = 64;
    auto tiny = unlock_rect(note, 30, 30, 192);
    CHECK((tiny == Rect{0,0,30,30}));
}

TEST_CASE("the same desktop lock control remains visible at the same location in both states") {
    Note note;
    for(int dpi:{96,144,192,240}) {
        note.locked=false;
        auto unlocked=unlock_rect(note,pixels(340,dpi),pixels(380,dpi),dpi);
        CHECK(window_spec(note).separate_unlock);
        note.locked=true;
        CHECK(window_spec(note).separate_unlock);
        CHECK(unlock_rect(note,pixels(340,dpi),pixels(380,dpi),dpi)==unlocked);
        CHECK(unlocked.right==pixels(340,dpi)-pixels(note.lock_offset,dpi));
    }
}

TEST_CASE("the header menu sits to the left of the fixed top-right lock control") {
    Note note;
    for(int dpi:{96,144,192,240}) {
        Layout layout{pixels(340,dpi),pixels(380,dpi),dpi};
        auto button=unlock_rect(note,layout.width,layout.height,dpi);
        CHECK(layout.hit(note,(button.left+button.right)/2,(button.top+button.bottom)/2).kind==HitKind::lock);
        CHECK(layout.hit(note,layout.width-pixels(52,dpi),pixels(22,dpi)).kind==HitKind::menu);
        CHECK(layout.hit(note,pixels(80,dpi),pixels(22,dpi)).kind==HitKind::title);
    }
}

TEST_CASE("header controls and footer fields share their alignment axes at multiple DPI scales") {
    Note note;
    for(int dpi:{96,144,192,240}) {
        Layout layout{pixels(340,dpi),pixels(380,dpi),dpi};
        auto lock=unlock_rect(note,layout.width,layout.height,dpi),menu=layout.menu_rect(note);
        CHECK(lock.top+lock.bottom==menu.top+menu.bottom);
        CHECK(menu.right<lock.left);
        auto filter=layout.filter_rect(),mode=layout.mode_rect();
        CHECK(filter.top==mode.top&&filter.bottom==mode.bottom&&filter.right<mode.left);
        auto input=layout.input_rect(),add=layout.add_rect();
        CHECK(input.top==add.top&&input.bottom==add.bottom&&input.right<add.left);
        CHECK(input.left==layout.width-add.right);
    }
}

TEST_CASE("monitor recovery fits missing-screen notes and scales dimensions on DPI change") {
    auto restored = fit_bounds({4000,2000,340,380,96}, {-1920,0,0,1040}, 144);
    CHECK(restored.width == 510 && restored.height == 570);
    CHECK(restored.x >= -1920 && restored.x + restored.width <= 0);
    CHECK(restored.y >= 0 && restored.y + restored.height <= 1040);
    auto small = fit_bounds({0,0,2000,2000,96}, {0,0,320,240}, 192);
    CHECK(small.width == 320 && small.height == 240);
}

TEST_CASE("hit testing cannot operate any part of a locked desktop body") {
    Model model;
    Id note = add_note(model);
    add_task(model, note, "Task");
    set_lock(model, note, true);
    Layout layout{340,380,96};
    CHECK(layout.hit(note_by_id(model,note), 20,20).kind == HitKind::none);
    CHECK(layout.hit(note_by_id(model,note), 36,90).kind == HitKind::none);
    set_lock(model, note, false);
    CHECK(layout.hit(note_by_id(model,note), 36,90).kind == HitKind::status);
    CHECK(layout.hit(note_by_id(model,note), 12,90).kind == HitKind::grip);
}

TEST_CASE("virtualized hit testing and drop targets use filtered task order after scrolling") {
    Model model;
    Id id = add_note(model);
    for (int i = 0; i < 30; ++i) add_task(model, id, std::to_string(i));
    auto& note = note_by_id(model,id);
    Layout layout{340,380,96,10};
    CHECK(layout.hit(note,100,layout.list_top()+10).task == note.tasks[10].id);
    CHECK(layout.drop_before(note,layout.list_top()) == note.tasks[10].id);
    CHECK(layout.page_rows() < 30);
    layout.scroll = 10000;
    CHECK(layout.clamped_scroll(note.tasks.size()) == 30 - layout.page_rows());
}
