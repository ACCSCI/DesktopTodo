const std = @import("std");
const sdk = @import("native_sdk");
const Bridge = @import("desktop_bridge.zig").Bridge;

test "desktop decorator preserves SDK callbacks without accessing Explorer headlessly" {
    const Delegate = struct {
        starts: usize = 0,
        events: usize = 0,
        stops: usize = 0,
        fn start(context: *anyopaque, _: *sdk.Runtime) !void {
            const self: *@This() = @ptrCast(@alignCast(context));
            self.starts += 1;
        }
        fn event(context: *anyopaque, _: *sdk.Runtime, _: sdk.Event) !void {
            const self: *@This() = @ptrCast(@alignCast(context));
            self.events += 1;
        }
        fn stop(context: *anyopaque, _: *sdk.Runtime) !void {
            const self: *@This() = @ptrCast(@alignCast(context));
            self.stops += 1;
        }
    };
    var delegate: Delegate = .{};
    var bridge: Bridge = .{
        .inner = .{ .context = &delegate, .name = "delegate", .start_fn = Delegate.start,
            .event_fn = Delegate.event, .stop_fn = Delegate.stop },
        // Must never be dereferenced during a headless callback.
        .ui = undefined,
    };
    const app = bridge.app();
    // Null source_fn is how the SDK recognizes native-only scenes. Adding
    // a no-op wrapper would accidentally activate its WebView boot path.
    try std.testing.expect(app.source_fn == null);
    try std.testing.expect(app.scene_fn == null);
    try std.testing.expect(app.replay_fn == null);
    const harness = try sdk.TestHarness().create(std.testing.allocator, .{});
    defer harness.destroy(std.testing.allocator);
    try app.start(&harness.runtime);
    try app.event(&harness.runtime, .{ .command = .{ .name = "probe" } });
    try app.stop(&harness.runtime);
    try std.testing.expectEqual(@as(usize, 1), delegate.starts);
    try std.testing.expectEqual(@as(usize, 1), delegate.events);
    try std.testing.expectEqual(@as(usize, 1), delegate.stops);
}
