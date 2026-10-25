const std = @import("std");
pub const Task = struct {
    bytes: [128]u8 = @splat(0),
    len: usize = 0,
    done: bool = false,
    pub fn text(self: *const Task) []const u8 { return self.bytes[0..self.len]; }
};
pub const Note = struct {
    tasks: [128]Task = @splat(.{}),
    count: usize = 0,
    unfinished_only: bool = false,
    locked: bool = false,
};
pub const Model = struct { notes: [10]Note = @splat(.{}), count: usize = 1 };
pub fn add(note: *Note, text: []const u8) bool {
    if (note.locked or note.count == note.tasks.len or text.len == 0 or text.len > 128 or !std.unicode.utf8ValidateSlice(text)) return false;
    var task: Task = .{};
    @memcpy(task.bytes[0..text.len],text);
    task.len = text.len;
    note.tasks[note.count] = task;
    note.count += 1;
    return true;
}
pub fn seed(model: *Model, notes: usize, tasks: usize) void {
    model.* = .{};
    model.count = notes;
    for (model.notes[0..notes]) |*note| for (0..tasks) |i| {
        var buffer: [64]u8 = undefined;
        _ = add(note,std.fmt.bufPrint(&buffer,"待办任务 {d}",.{i+1}) catch unreachable);
    };
}
pub fn toggle(note: *Note, index: usize) void {
    if (!note.locked and index < note.count) note.tasks[index].done = !note.tasks[index].done;
}
pub fn visible(note: *const Note, index: usize) bool {
    return index < note.count and (!note.unfinished_only or !note.tasks[index].done);
}
test "two workload sizes contain independent real Todo state" {
    var model: Model = .{};
    seed(&model,1,10);
    try std.testing.expectEqual(@as(usize,10),model.notes[0].count);
    seed(&model,10,100);
    try std.testing.expectEqual(@as(usize,10),model.count);
    toggle(&model.notes[0],0);
    try std.testing.expect(model.notes[0].tasks[0].done);
    try std.testing.expect(!model.notes[1].tasks[0].done);
}
test "Unicode add, native filtering and lock rules work without UI" {
    var note: Note = .{};
    try std.testing.expect(add(&note,"中文 🎯"));
    try std.testing.expectEqualStrings("中文 🎯",note.tasks[0].text());
    toggle(&note,0);
    note.unfinished_only = true;
    try std.testing.expect(!visible(&note,0));
    note.locked = true;
    toggle(&note,0);
    try std.testing.expect(note.tasks[0].done);
    try std.testing.expect(!add(&note,"不能添加"));
}
