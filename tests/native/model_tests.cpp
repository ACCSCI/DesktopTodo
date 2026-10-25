#include "test.hpp"
#include "todo/actions.hpp"
#include <limits>
using namespace todo;

struct Tasks {
    Model model;
    Id note = add_note(model, "工作"), a = add_task(model, note, "A"),
       b = add_task(model, note, "B"), c = add_task(model, note, "C"), d = add_task(model, note, "D");
    Note& get() { return note_by_id(model, note); }
};
static std::vector<Id> order(const Note& note) {
    std::vector<Id> ids;
    for (const auto& task : note.tasks) ids.push_back(task.id);
    return ids;
}

TEST_CASE("multiple notes retain independent titles, colors, modes and tasks") {
    Model model;
    Id work = add_note(model, " 工作 "), life = add_note(model, "生活");
    add_task(model, work, "完成项目");
    set_note_color(model, work, 0xABCDEF);
    set_mode(model, life, Mode::window);
    rename_note(model, life, "生活安排");
    CHECK(note_by_id(model, work).title == "工作");
    CHECK(note_by_id(model, life).title == "生活安排");
    CHECK(note_by_id(model, life).color != 0xABCDEF);
    CHECK(note_by_id(model, work).tasks.size() == 1);
    CHECK(note_by_id(model, life).tasks.empty());
    validate(model);
}

TEST_CASE("UTF-8 Chinese and emoji text are preserved and invalid input is rejected") {
    Model model;
    Id note = add_note(model, "中文便签 🎯");
    Id task = add_task(model, note, "  给朋友打电话 ☎  ");
    set_task_icon(model, note, task, "🌟");
    edit_task(model, note, task, "买牛奶与面包 🥛");
    CHECK(task_by_id(note_by_id(model, note), task).text == "买牛奶与面包 🥛");
    CHECK(task_by_id(note_by_id(model, note), task).icon == "🌟");
    THROWS_AS(add_task(model, note, " \t\r\n "), std::invalid_argument);
    THROWS_AS(add_task(model, note, std::string("a\0b", 3)), std::invalid_argument);
    THROWS_AS(add_task(model, note, std::string("\xC0\xAF", 2)), std::invalid_argument);
    THROWS_AS(add_task(model, note, std::string("\xED\xA0\x80", 3)), std::invalid_argument);
    THROWS_AS(add_task(model, note, std::string(8193, 'x')), std::invalid_argument);
    validate(model);
}

TEST_CASE("status transitions and completion time are independent of task icon and color") {
    Tasks t;
    set_task_icon(t.model, t.note, t.a, "☎");
    set_task_color(t.model, t.note, t.a, 0x1456FA);
    set_status(t.model, t.note, t.a, Status::doing, 100);
    CHECK(task_by_id(t.get(), t.a).completed == 0);
    set_status(t.model, t.note, t.a, Status::done, 120);
    CHECK(task_by_id(t.get(), t.a).completed == 120);
    set_status(t.model, t.note, t.a, Status::done, 900);
    CHECK(task_by_id(t.get(), t.a).completed == 120);
    set_status(t.model, t.note, t.a, Status::pending, 901);
    CHECK(task_by_id(t.get(), t.a).completed == 0);
    CHECK(task_by_id(t.get(), t.a).icon == "☎");
    CHECK(task_by_id(t.get(), t.a).color == 0x1456FA);
}

TEST_CASE("all five filters include precisely the intended task statuses") {
    Tasks t;
    set_status(t.model, t.note, t.b, Status::doing);
    set_status(t.model, t.note, t.c, Status::done);
    set_filter(t.model, t.note, Filter::all);
    CHECK((visible_tasks(t.get()) == std::vector<Id>{t.a,t.b,t.c,t.d}));
    set_filter(t.model, t.note, Filter::unfinished);
    CHECK((visible_tasks(t.get()) == std::vector<Id>{t.a,t.b,t.d}));
    set_filter(t.model, t.note, Filter::pending);
    CHECK((visible_tasks(t.get()) == std::vector<Id>{t.a,t.d}));
    set_filter(t.model, t.note, Filter::doing);
    CHECK((visible_tasks(t.get()) == std::vector<Id>{t.b}));
    set_filter(t.model, t.note, Filter::done);
    CHECK((visible_tasks(t.get()) == std::vector<Id>{t.c}));
    CHECK(t.get().tasks.size() == 4);
}

TEST_CASE("dragging before a task and to the end saves a stable manual order") {
    Tasks t;
    reorder_visible(t.model, t.note, t.d, t.b);
    CHECK((order(t.get()) == std::vector<Id>{t.a,t.d,t.b,t.c}));
    reorder_visible(t.model, t.note, t.a, 0);
    CHECK((order(t.get()) == std::vector<Id>{t.d,t.b,t.c,t.a}));
    auto unchanged = t.model;
    reorder_visible(t.model, t.note, t.b, t.b);
    CHECK(t.model == unchanged);
}

TEST_CASE("filtered sorting keeps hidden tasks in their original slots") {
    Tasks t;
    set_status(t.model, t.note, t.b, Status::done);
    set_filter(t.model, t.note, Filter::unfinished);
    reorder_visible(t.model, t.note, t.d, t.a);
    CHECK((order(t.get()) == std::vector<Id>{t.d,t.b,t.a,t.c}));
    auto old = t.model;
    THROWS_AS(reorder_visible(t.model, t.note, t.b, t.d), std::invalid_argument);
    THROWS_AS(reorder_visible(t.model, t.note, t.c, t.b), std::invalid_argument);
    CHECK(t.model == old);
}

TEST_CASE("moving tasks between notes preserves their identity, status and styling") {
    Tasks t;
    Id other = add_note(t.model, "另一个便签");
    Id last = add_task(t.model, other, "Existing");
    set_status(t.model, t.note, t.b, Status::doing);
    set_task_icon(t.model, t.note, t.b, "★");
    auto original = task_by_id(t.get(), t.b);
    move_task(t.model, t.note, other, t.b, last);
    CHECK(t.get().tasks.size() == 3);
    CHECK(note_by_id(t.model, other).tasks.front() == original);
    validate(t.model);
}

TEST_CASE("deleting a note and removing completed tasks do not change other notes") {
    Tasks t;
    Id other = add_note(t.model, "Keep");
    Id retained = add_task(t.model, other, "Kept task");
    set_status(t.model, t.note, t.a, Status::done);
    remove_completed(t.model, t.note);
    CHECK(t.get().tasks.size() == 3);
    remove_note(t.model, t.note);
    CHECK(task_by_id(note_by_id(t.model, other), retained).text == "Kept task");
    validate(t.model);
}

TEST_CASE("desktop startup lock affects desktop notes only and can be disabled") {
    Model model;
    Id desktop = add_note(model), top = add_note(model), window = add_note(model);
    set_mode(model, top, Mode::topmost);
    set_mode(model, window, Mode::window);
    restore_startup_policy(model);
    CHECK(note_by_id(model, desktop).locked);
    CHECK(!note_by_id(model, top).locked);
    CHECK(!note_by_id(model, window).locked);
    model.lock_on_start = false;
    set_lock(model, desktop, false);
    restore_startup_policy(model);
    CHECK(!note_by_id(model, desktop).locked);
}

TEST_CASE("lock positions, negative display coordinates and colors are validated") {
    Model model;
    Id note = add_note(model);
    for (auto corner : {Corner::top_left, Corner::top_right, Corner::bottom_left, Corner::bottom_right}) {
        set_lock_position(model, note, corner, 8);
        CHECK(note_by_id(model, note).corner == corner);
    }
    set_bounds(model, note, {-1200,40,340,500,144});
    CHECK(note_by_id(model, note).bounds.x == -1200);
    THROWS_AS(set_bounds(model, note, {0,0,10,20,96}), std::invalid_argument);
    THROWS_AS(set_note_color(model, note, 0x1000000), std::invalid_argument);
    THROWS_AS(set_lock_position(model, note, Corner::top_right, 80), std::invalid_argument);
    validate(model);
}

TEST_CASE("invalid references and exhausted IDs fail without creating phantom data") {
    Model model;
    THROWS_AS(add_task(model, 123, "Missing note"), std::out_of_range);
    CHECK(model.notes.empty());
    model.next_id = std::numeric_limits<Id>::max();
    THROWS_AS(add_note(model), std::overflow_error);
    CHECK(model.notes.empty());
}

TEST_CASE("validation rejects duplicate IDs and corrupted model enums") {
    Tasks t;
    t.get().tasks[1].id = t.get().tasks[0].id;
    THROWS_AS(validate(t.model), std::invalid_argument);
    t.get().tasks[1].id = t.b;
    t.get().mode = Mode(99);
    THROWS_AS(validate(t.model), std::invalid_argument);
}
