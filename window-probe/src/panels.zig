const std = @import("std");
const main = @import("main.zig");
const App = main.CounterApp;

pub fn windows(model: *const main.Model, scratch: *App.WindowsScratch) []const App.WindowDescriptor {
    if (!model.overlay_open) return scratch.windows[0..0];
    scratch.windows[0] = .{
        // SDK overlay policies are immutable: a changed identity replaces the
        // native window when lock changes, instead of pretending flags mutate.
        .label = model.mode.bodyLabel(model.locked),
        .canvas_label = "note-canvas",
        .title = "Todo Overlay Probe",
        .width = 300,
        .height = 180,
        .resizable = model.mode == .window,
        .x = 540,
        .y = 120,
        .titlebar = if (model.mode == .window) .standard else .chromeless,
        .transparent = model.mode != .window,
        .always_on_top = model.mode == .topmost,
        .click_through = model.locked,
        .activate_on_show = false,
    };
    scratch.windows[1] = .{
        .label = model.mode.controlLabel(),
        .canvas_label = "unlock-canvas",
        .title = "Todo Unlock Probe",
        .width = 90,
        .height = 40,
        .resizable = false,
        .x = 750,
        .y = 120,
        .titlebar = .chromeless,
        .transparent = true,
        .always_on_top = model.mode == .topmost,
        .click_through = false,
        .activate_on_show = false,
    };
    return scratch.windows[0..2];
}

pub fn view(ui: *App.Ui, model: *const main.Model, label: []const u8) App.Ui.Node {
    if (std.mem.endsWith(u8, label, "unlock-control")) {
        return ui.column(.{ .padding = 4 }, .{
            ui.button(.{ .on_press = .toggle_lock }, if (model.locked) "Unlock" else "Lock"),
        });
    }
    return ui.column(.{ .padding = 16, .gap = 12 }, .{
        ui.text(.{}, "Todo overlay feasibility probe"),
        ui.text(.{}, if (model.locked) "Locked body; independent unlock" else "Unlocked body"),
        ui.button(.{ .on_press = .increment }, "Increment from note"),
        ui.text(.{}, ui.fmt("Count: {d}", .{model.count})),
    });
}
