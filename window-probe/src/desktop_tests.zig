const std = @import("std");
const policy = @import("window_mode.zig");

const FakePort = struct {
    state: policy.Snapshot = .{
        .parent = 0, .style = policy.popup_style,
        .extended = policy.topmost_style | policy.appwindow_style | 0x80020,
        .rect = .{ .x = -200, .y = 120, .width = 600, .height = 360 },
    },
    failure: enum { none, styles, parent, position } = .none,
    restored: bool = false,
    positioned_parent: usize = 0,

    pub fn capture(self: *@This(), _: usize) !policy.Snapshot { return self.state; }
    pub fn styles(self: *@This(), _: usize, style: u32, extended: u32) !void {
        self.state.style = style;
        self.state.extended = extended;
        if (self.failure == .styles) return error.InjectedFailure;
    }
    pub fn reparent(self: *@This(), _: usize, parent: usize) !void {
        self.state.parent = parent;
        if (self.failure == .parent) return error.InjectedFailure;
    }
    pub fn position(self: *@This(), _: usize, parent: usize, rect: policy.Rect) !void {
        self.positioned_parent = parent;
        self.state.rect = rect;
        if (self.failure == .position) return error.InjectedFailure;
    }
    pub fn restore(self: *@This(), _: usize, old: policy.Snapshot) void {
        self.state = old;
        self.restored = true;
    }
};

test "desktop attachment clears popup/topmost/taskbar flags but preserves hit-test policy" {
    var port: FakePort = .{};
    const original_rect = port.state.rect;
    try policy.attach(&port, 100, 200);
    try std.testing.expectEqual(@as(usize, 200), port.state.parent);
    try std.testing.expect(port.state.style & policy.child_style != 0);
    try std.testing.expect(port.state.style & policy.popup_style == 0);
    try std.testing.expect(port.state.extended & policy.topmost_style == 0);
    try std.testing.expect(port.state.extended & policy.appwindow_style == 0);
    try std.testing.expectEqual(@as(u32, 0x80020), port.state.extended & 0x80020);
    try std.testing.expectEqual(original_rect, port.state.rect);
}

test "each attachment failure rolls back native state" {
    for ([_]@FieldType(FakePort, "failure"){ .styles, .parent, .position }) |failure| {
        var port: FakePort = .{ .failure = failure };
        const old = port.state;
        try std.testing.expectError(error.InjectedFailure, policy.attach(&port, 100, 200));
        try std.testing.expect(port.restored);
        try std.testing.expectEqual(old, port.state);
    }
}

test "invalid attachment never touches the backend" {
    var port: FakePort = .{};
    try std.testing.expectError(error.InvalidHandle, policy.attach(&port, 0, 200));
    try std.testing.expectError(error.InvalidHandle, policy.attach(&port, 100, 100));
    try std.testing.expect(!port.restored);
}

test "mode changes replace both window identities" {
    try std.testing.expect(!std.mem.eql(u8, policy.Mode.desktop.bodyLabel(true), policy.Mode.topmost.bodyLabel(true)));
    try std.testing.expect(!std.mem.eql(u8, policy.Mode.desktop.controlLabel(), policy.Mode.topmost.controlLabel()));
}

test "new bodies raise the surviving control once, without recurring foreground stealing" {
    try std.testing.expect(policy.controlsNeedRaise(1, 2, 3, 2));
    try std.testing.expect(policy.controlsNeedRaise(0, 0, 1, 2));
    try std.testing.expect(!policy.controlsNeedRaise(1, 2, 1, 2));
    try std.testing.expect(!policy.controlsNeedRaise(1, 2, 0, 2));
}

test "authored positions scale across DPI and retain negative monitor coordinates" {
    try std.testing.expectEqual(@as(i32, 1080), policy.physicalPosition(540, 192));
    try std.testing.expectEqual(@as(i32, -300), policy.physicalPosition(-200, 144));
}
