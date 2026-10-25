const std = @import("std");
const main = @import("main.zig");
const panels = @import("panels.zig");
const Mode = @import("window_mode.zig").Mode;

test "lock leaves a separate interactive control and recreates body identity" {
    var scratch: main.CounterApp.WindowsScratch = undefined;
    var model = main.initialModel();
    try std.testing.expectEqual(@as(usize, 0), panels.windows(&model, &scratch).len);
    model.overlay_open = true;
    const locked = panels.windows(&model, &scratch);
    try std.testing.expectEqual(@as(usize, 2), locked.len);
    try std.testing.expect(locked[0].click_through);
    try std.testing.expect(!locked[1].click_through);
    try std.testing.expectEqualStrings("note-locked", locked[0].label);
    model.locked = false;
    const unlocked = panels.windows(&model, &scratch);
    try std.testing.expect(!unlocked[0].click_through);
    try std.testing.expectEqualStrings("note-unlocked", unlocked[0].label);
    try std.testing.expectEqualStrings("unlock-control", unlocked[1].label);
}

test "mode commands preserve note state and declare both native window policies" {
    var fx = main.Effects.init(std.testing.allocator);
    defer fx.deinit();
    fx.executor = .fake;
    var model = main.initialModel();
    model.overlay_open = true;
    model.count = 7;
    model.locked = false;
    var scratch: main.CounterApp.WindowsScratch = undefined;
    const messages = [_]main.Msg{ .mode_desktop, .mode_topmost, .mode_window };
    const modes = [_]Mode{ .desktop, .topmost, .window };
    for (messages, modes) |message, mode| {
        main.update(&model, message, &fx);
        try std.testing.expectEqual(mode, model.mode);
        try std.testing.expectEqual(@as(i64, 7), model.count);
        try std.testing.expect(!model.locked);
        const windows = panels.windows(&model, &scratch);
        try std.testing.expectEqual(@as(usize, 2), windows.len);
        try std.testing.expectEqual(mode == .topmost, windows[0].always_on_top);
        try std.testing.expectEqual(mode == .topmost, windows[1].always_on_top);
        try std.testing.expectEqual(mode != .window, windows[0].transparent);
        try std.testing.expect(!windows[1].click_through);
    }
}
