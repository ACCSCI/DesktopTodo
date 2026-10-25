#pragma once
#include "model.hpp"

namespace todo {
Id add_note(Model& model, const std::string& title = "我的待办");
void remove_note(Model& model, Id note);
void rename_note(Model& model, Id note, const std::string& title);
Id add_task(Model& model, Id note, const std::string& text, std::int64_t now = 0);
void remove_task(Model& model, Id note, Id task);
void edit_task(Model& model, Id note, Id task, const std::string& text);
void set_status(Model& model, Id note, Id task, Status status, std::int64_t now = 0);
void set_task_icon(Model& model, Id note, Id task, const std::string& icon);
void set_task_color(Model& model, Id note, Id task, Color color);
void set_note_color(Model& model, Id note, Color color);
void set_filter(Model& model, Id note, Filter filter);
void set_mode(Model& model, Id note, Mode mode);
void set_lock(Model& model, Id note, bool locked);
void set_lock_position(Model& model, Id note, Corner corner, int offset);
void set_bounds(Model& model, Id note, Bounds bounds);
// before=0 appends. Hidden tasks retain their exact slots in filtered views.
void reorder_visible(Model& model, Id note, Id task, Id before);
void move_task(Model& model, Id source, Id target, Id task, Id before = 0);
void remove_completed(Model& model, Id note);
void restore_startup_policy(Model& model);
}
