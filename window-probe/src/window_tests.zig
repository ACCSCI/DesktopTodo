const std = @import("std");
const sdk = @import("native_sdk");

test "headless runtime preserves normal and overlay window policies independently" {
    const harness = try sdk.TestHarness().create(std.testing.allocator, .{});
    defer harness.destroy(std.testing.allocator);

    const normal = try harness.runtime.createWindow(.{
        .label = "normal-note",
        .title = "Normal note",
        .source = sdk.platform.WebViewSource.html("<p>Normal</p>"),
        .default_frame = sdk.geometry.RectF.init(40, 40, 320, 240),
    });
    const locked = try harness.runtime.createWindow(.{
        .label = "locked-note",
        .title = "Locked note",
        .titlebar = .chromeless,
        .transparent = true,
        .always_on_top = true,
        .click_through = true,
        .activate_on_show = false,
        .default_frame = sdk.geometry.RectF.init(400, 40, 320, 240),
    });
    const unlock = try harness.runtime.createWindow(.{
        .label = "unlock-button",
        .titlebar = .chromeless,
        .transparent = true,
        .always_on_top = true,
        .click_through = false,
        .activate_on_show = false,
        .default_frame = sdk.geometry.RectF.init(680, 40, 40, 40),
    });
    try std.testing.expect(normal.id != locked.id);
    try std.testing.expect(locked.id != unlock.id);
    // The null host stores creation options by window slot, just as the
    // Windows host receives them. This tests the contract, not OS hit testing.
    try std.testing.expect(!harness.null_platform.window_always_on_top[0]);
    try std.testing.expect(harness.null_platform.window_always_on_top[1]);
    try std.testing.expect(harness.null_platform.window_click_through[1]);
    try std.testing.expect(!harness.null_platform.window_activate_on_show[1]);
    try std.testing.expect(!harness.null_platform.window_click_through[2]);
    try std.testing.expect(harness.null_platform.window_always_on_top[2]);
}
