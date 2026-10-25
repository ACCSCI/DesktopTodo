#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace todo {
using Id = std::int64_t;
using Color = std::uint32_t; // 0xRRGGBB, independent of Win32 COLORREF.
enum class Status { pending, doing, done };
enum class Filter { all, unfinished, pending, doing, done };
enum class Mode { desktop, topmost, window };
enum class Corner { top_left, top_right, bottom_left, bottom_right };

struct Bounds {
    int x = 80, y = 80, width = 340, height = 380, dpi = 96;
    bool operator==(const Bounds&) const = default;
};
struct Task {
    Id id = 0;
    std::string text;
    std::string icon = "•";
    Color color = 0x334155;
    Status status = Status::pending;
    std::int64_t created = 0, completed = 0;
    bool operator==(const Task&) const = default;
};
struct Note {
    Id id = 0;
    std::string title = "我的待办";
    Color color = 0xFFF4C2;
    Mode mode = Mode::desktop;
    bool locked = false;
    Corner corner = Corner::top_right;
    int lock_offset = 4;
    Filter filter = Filter::all;
    Bounds bounds;
    std::vector<Task> tasks;
    bool operator==(const Note&) const = default;
};
struct Model {
    Id next_id = 1;
    bool lock_on_start = true;
    std::vector<Note> notes;
    bool operator==(const Model&) const = default;
};

bool valid_utf8(const std::string& text);
std::string clean_text(const std::string& text, std::size_t limit);
bool matches(Filter filter, Status status);
std::vector<Id> visible_tasks(const Note& note);
Note& note_by_id(Model& model, Id id);
const Note& note_by_id(const Model& model, Id id);
Task& task_by_id(Note& note, Id id);
const Task& task_by_id(const Note& note, Id id);
void validate(const Model& model);
}
