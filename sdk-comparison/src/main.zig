const std = @import("std");
const native = @import("native_sdk");
const runner = @import("runner");
const core = @import("core.zig");
pub const panic = std.debug.FullPanic(native.debug.capturePanic);
const canvas = native.canvas;
pub const Model = struct {
    todos: core.Model = .{},
    drafts: [10]canvas.TextBuffer(128) = @splat(.{}),
};
pub const Msg = union(enum) {
    small, large,
    toggle: struct { note: usize, task: usize },
    filter: usize, lock: usize, add: usize,
    edit: struct { note: usize, event: canvas.TextInputEvent },
};
pub const App = native.UiApp(Model,Msg);
pub fn update(model: *Model, msg: Msg) void {
    switch (msg) {
        .small => core.seed(&model.todos,1,10),
        .large => core.seed(&model.todos,5,100),
        .toggle => |value| core.toggle(&model.todos.notes[value.note],value.task),
        .filter => |i| model.todos.notes[i].unfinished_only = !model.todos.notes[i].unfinished_only,
        .lock => |i| model.todos.notes[i].locked = !model.todos.notes[i].locked,
        .edit => |value| if (!model.todos.notes[value.note].locked) { model.drafts[value.note].apply(value.event); },
        .add => |i| if (core.add(&model.todos.notes[i],model.drafts[i].text())) { model.drafts[i] = .{}; },
    }
}
const labels = [_][]const u8{ "main", "note-2", "note-3", "note-4", "note-5", "note-6", "note-7", "note-8", "note-9", "note-10" };
const canvases = [_][]const u8{ "main-canvas", "canvas-2", "canvas-3", "canvas-4", "canvas-5", "canvas-6", "canvas-7", "canvas-8", "canvas-9", "canvas-10" };
pub fn windows(model: *const Model, scratch: *App.WindowsScratch) []const App.WindowDescriptor {
    for (1..model.todos.count) |i| scratch.windows[i-1] = .{
        .label=labels[i], .canvas_label=canvases[i], .title="Native SDK Todo",
        .width=340, .height=380, .x=80, .y=80, .activate_on_show=false,
    };
    return scratch.windows[0..model.todos.count-1];
}
fn input(comptime i: usize) App.Ui.InputMsgFn {
    return struct { fn make(event: canvas.TextInputEvent) Msg { return .{.edit=.{.note=i,.event=event}}; } }.make;
}
pub fn noteView(ui: *App.Ui, model: *const Model, i: usize) App.Ui.Node {
    const note = &model.todos.notes[i];
    var rows: [128]App.Ui.Node = undefined;
    var count: usize = 0;
    for (note.tasks[0..note.count],0..) |*task,t| {
        if (!core.visible(note,t)) continue;
        rows[count] = ui.row(.{ .key=.{.int=t}, .gap=10, .height=38, .cross=.center }, .{
            ui.checkbox(.{ .checked=task.done, .disabled=note.locked, .on_toggle=.{.toggle=.{.note=i,.task=t}}, .semantics=.{.label="完成待办"} }),
            ui.text(.{ .grow=1 },task.text()),
            ui.text(.{},if (task.done) "已完成" else "待办"),
        });
        count += 1;
    }
    var on_input: App.Ui.InputMsgFn = input(0);
    inline for (0..10) |index| if (i == index) { on_input = input(index); };
    return ui.column(.{ .padding=16, .gap=10 }, .{
        ui.row(.{ .gap=8, .cross=.center }, .{
            ui.text(.{ .grow=1 },ui.fmt("我的待办 {d}",.{i+1})),
            ui.button(.{ .width=60, .on_press=.{.lock=i} },if (note.locked) "解锁" else "锁定"),
        }),
        ui.row(.{ .gap=6 }, .{
            ui.button(.{ .size=.sm, .on_press=.{.filter=i} },if (note.unfinished_only) "显示全部" else "未完成"),
            ui.button(.{ .size=.sm, .on_press=.small },"1×10"),
            ui.button(.{ .size=.sm, .on_press=.large },"5×100"),
        }),
        ui.scroll(.{ .grow=1 }, .{ui.column(.{ .gap=2 },rows[0..count])}),
        ui.row(.{ .gap=8 }, .{
            ui.textField(.{ .text=model.drafts[i].text(), .placeholder="添加待办…", .grow=1, .disabled=note.locked, .on_input=on_input, .on_submit=.{.add=i} }),
            ui.button(.{ .variant=.primary, .disabled=note.locked, .on_press=.{.add=i} },"添加"),
        }),
        ui.text(.{},ui.fmt("{d} 项 · Native SDK / Geist",.{note.count})),
    });
}
fn mainView(ui: *App.Ui, model: *const Model) App.Ui.Node { return noteView(ui,model,0); }
fn windowView(ui: *App.Ui, model: *const Model, label: []const u8) App.Ui.Node {
    for (labels,0..) |name,i| if (std.mem.eql(u8,name,label)) { return noteView(ui,model,i); };
    return noteView(ui,model,0);
}
pub fn main(init: std.process.Init) !void {
    const views = [_]native.ShellView{.{ .label="main-canvas", .kind=.gpu_surface, .fill=true }};
    const scene_windows = [_]native.ShellWindow{.{ .label="main", .title="Native SDK Todo", .width=340, .height=380, .views=&views }};
    const app = try App.create(std.heap.page_allocator,.{
        .name="sdk-comparison", .theme=.geist, .scene=.{.windows=&scene_windows},
        .canvas_label="main-canvas", .update=update, .view=mainView,
        .windows_fn=windows, .window_view=windowView,
    });
    defer app.destroy();
    core.seed(&app.model.todos,1,10);
    try runner.runWithOptions(app.app(),.{
        .app_name="sdk-comparison", .window_title="Native SDK Todo", .bundle_id="dev.desktoptodo.sdk-comparison",
        .default_frame=native.geometry.RectF.init(80,80,340,380), .js_window_api=false,
        .security=.{.permissions=&.{native.security.permission_view,native.security.permission_command}},
    },init);
}
test { _ = @import("core.zig"); _ = @import("view_tests.zig"); }
