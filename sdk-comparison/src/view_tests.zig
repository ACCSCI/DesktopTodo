const std = @import("std");
const native = @import("native_sdk");
const main = @import("main.zig");
fn find(widget: native.canvas.Widget, text: []const u8) ?native.canvas.Widget {
    if (widget.kind == .button and std.mem.eql(u8,widget.text,text)) return widget;
    for (widget.children) |child| if (find(child,text)) |result| { return result; };
    return null;
}
test "real SDK button dispatch creates exactly five supported native note windows" {
    const model = try std.testing.allocator.create(main.Model);
    defer std.testing.allocator.destroy(model);
    model.* = .{};
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    var ui = main.App.Ui.init(arena.allocator());
    const tree = try ui.finalize(main.noteView(&ui,model,0));
    const button = find(tree.root,"5×100") orelse return error.MissingButton;
    main.update(model,tree.msgForPointer(button.id,.up).?);
    var scratch: main.App.WindowsScratch = undefined;
    try std.testing.expectEqual(@as(usize,4),main.windows(model,&scratch).len);
    try std.testing.expectEqual(@as(usize,100),model.todos.notes[4].count);
}
test "SDK note trees contain real task controls and lock blocks mutations" {
    const model = try std.testing.allocator.create(main.Model);
    defer std.testing.allocator.destroy(model);
    model.* = .{};
    main.update(model,.small);
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    var ui = main.App.Ui.init(arena.allocator());
    const tree = try ui.finalize(main.noteView(&ui,model,0));
    const lock = find(tree.root,"锁定") orelse return error.MissingButton;
    main.update(model,tree.msgForPointer(lock.id,.up).?);
    main.update(model,.{.toggle=.{.note=0,.task=0}});
    try std.testing.expect(!model.todos.notes[0].tasks[0].done);
    try std.testing.expect(model.todos.notes[0].locked);
}
