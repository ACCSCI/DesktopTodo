#include "todo/actions.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace todo {
namespace {
Id allocate(Model& model) {
    if (model.next_id <= 0 || model.next_id == std::numeric_limits<Id>::max()) throw std::overflow_error("ID space exhausted");
    return model.next_id++;
}
void color_valid(Color color) { if (color > 0xFFFFFF) throw std::invalid_argument("Invalid color"); }
}
Id add_note(Model& model, const std::string& title) {
    auto text = clean_text(title, 512);
    Note note;
    note.id = allocate(model);
    note.title = std::move(text);
    note.bounds.x += int(model.notes.size() % 8) * 28;
    note.bounds.y += int(model.notes.size() % 8) * 28;
    auto id = note.id;
    model.notes.push_back(std::move(note));
    return id;
}
void remove_note(Model& model, Id id) {
    note_by_id(model, id);
    std::erase_if(model.notes, [id](const Note& n) { return n.id == id; });
}
void rename_note(Model& model, Id id, const std::string& title) {
    auto cleaned = clean_text(title, 512);
    note_by_id(model, id).title = std::move(cleaned);
}
Id add_task(Model& model, Id id, const std::string& text, std::int64_t now) {
    auto cleaned = clean_text(text, 8192);
    if (now < 0) throw std::invalid_argument("Invalid timestamp");
    auto& note = note_by_id(model, id);
    Task task;
    task.id = allocate(model);
    task.text = std::move(cleaned);
    task.created = now;
    auto result = task.id;
    note.tasks.push_back(std::move(task));
    return result;
}
void remove_task(Model& model, Id note, Id id) {
    auto& tasks = note_by_id(model, note).tasks;
    task_by_id(note_by_id(model, note), id);
    std::erase_if(tasks, [id](const Task& t) { return t.id == id; });
}
void edit_task(Model& model, Id note, Id id, const std::string& text) {
    auto cleaned = clean_text(text, 8192);
    task_by_id(note_by_id(model, note), id).text = std::move(cleaned);
}
void set_status(Model& model, Id note, Id id, Status status, std::int64_t now) {
    if (int(status) < 0 || int(status) > 2 || now < 0) throw std::invalid_argument("Invalid status");
    auto& task = task_by_id(note_by_id(model, note), id);
    if (task.status == status) return;
    task.status = status;
    task.completed = status == Status::done ? now : 0;
}
void set_task_icon(Model& model, Id note, Id id, const std::string& icon) {
    auto cleaned = clean_text(icon, 64);
    task_by_id(note_by_id(model, note), id).icon = std::move(cleaned);
}
void set_task_color(Model& model, Id note, Id id, Color color) {
    color_valid(color);
    task_by_id(note_by_id(model, note), id).color = color;
}
void set_note_color(Model& model, Id note, Color color) {
    color_valid(color);
    note_by_id(model, note).color = color;
}
void set_filter(Model& model, Id note, Filter filter) {
    if (int(filter) < 0 || int(filter) > 4) throw std::invalid_argument("Invalid filter");
    note_by_id(model, note).filter = filter;
}
void set_mode(Model& model, Id note, Mode mode) {
    if (int(mode) < 0 || int(mode) > 2) throw std::invalid_argument("Invalid mode");
    note_by_id(model, note).mode = mode;
}
void set_lock(Model& model, Id note, bool locked) { note_by_id(model, note).locked = locked; }
void set_lock_position(Model& model, Id note, Corner corner, int offset) {
    if (int(corner) < 0 || int(corner) > 3 || offset < 0 || offset > 64) throw std::invalid_argument("Invalid lock position");
    auto& target = note_by_id(model, note);
    target.corner = corner;
    target.lock_offset = offset;
}
void set_bounds(Model& model, Id note, Bounds bounds) {
    // Reuse full validation without partially updating a live model.
    auto candidate = model;
    note_by_id(candidate, note).bounds = bounds;
    validate(candidate);
    note_by_id(model, note).bounds = bounds;
}
void reorder_visible(Model& model, Id note_id, Id task_id, Id before) {
    if (task_id == before) return;
    auto& note = note_by_id(model, note_id);
    std::vector<std::size_t> slots;
    std::vector<Task> visible;
    for (std::size_t i = 0; i < note.tasks.size(); ++i) {
        if (matches(note.filter, note.tasks[i].status)) { slots.push_back(i); visible.push_back(note.tasks[i]); }
    }
    auto source = std::find_if(visible.begin(), visible.end(), [=](const Task& t) { return t.id == task_id; });
    if (source == visible.end()) throw std::invalid_argument("Dragged task is not visible");
    if (before && std::none_of(visible.begin(), visible.end(), [=](const Task& t) { return t.id == before; }))
        throw std::invalid_argument("Drop target is not visible");
    Task moved = *source;
    visible.erase(source);
    auto target = std::find_if(visible.begin(), visible.end(), [=](const Task& t) { return t.id == before; });
    visible.insert(target, std::move(moved));
    for (std::size_t i = 0; i < slots.size(); ++i) note.tasks[slots[i]] = std::move(visible[i]);
}
void move_task(Model& model, Id source, Id target, Id id, Id before) {
    if (source == target) { reorder_visible(model, source, id, before); return; }
    auto& origin = note_by_id(model, source);
    auto& destination = note_by_id(model, target);
    auto copy = task_by_id(origin, id);
    auto position = std::find_if(destination.tasks.begin(), destination.tasks.end(), [=](const Task& t) { return t.id == before; });
    if (before && position == destination.tasks.end()) throw std::invalid_argument("Drop target does not exist");
    destination.tasks.insert(position, std::move(copy));
    std::erase_if(origin.tasks, [id](const Task& t) { return t.id == id; });
}
void remove_completed(Model& model, Id note) {
    std::erase_if(note_by_id(model, note).tasks, [](const Task& t) { return t.status == Status::done; });
}
void restore_startup_policy(Model& model) {
    if (model.lock_on_start) for (auto& note : model.notes) if (note.mode == Mode::desktop) note.locked = true;
}
}
