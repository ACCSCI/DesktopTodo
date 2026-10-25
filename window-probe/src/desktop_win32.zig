//! Win32 boundary: change only windows owned by this process. Explorer's
//! existing desktop icon host is reused, never reconfigured or destroyed.
const std = @import("std");
const policy = @import("window_mode.zig");
const wide = std.unicode.utf8ToUtf16LeStringLiteral;
const Rect = extern struct { left: i32, top: i32, right: i32, bottom: i32 };
const Point = extern struct { x: i32, y: i32 };
const Callback = *const fn (usize, isize) callconv(.c) c_int;

extern "user32" fn EnumWindows(Callback, isize) callconv(.c) c_int;
extern "user32" fn FindWindowExW(usize, usize, ?[*:0]const u16, ?[*:0]const u16) callconv(.c) usize;
extern "user32" fn GetWindowTextW(usize, [*]u16, c_int) callconv(.c) c_int;
extern "user32" fn GetWindowThreadProcessId(usize, *u32) callconv(.c) u32;
extern "user32" fn GetParent(usize) callconv(.c) usize;
extern "user32" fn GetWindowLongPtrW(usize, c_int) callconv(.c) isize;
extern "user32" fn SetWindowLongPtrW(usize, c_int, isize) callconv(.c) isize;
extern "user32" fn SetParent(usize, usize) callconv(.c) usize;
extern "user32" fn GetWindowRect(usize, *Rect) callconv(.c) c_int;
extern "user32" fn MapWindowPoints(usize, usize, *Point, u32) callconv(.c) c_int;
extern "user32" fn SetWindowPos(usize, usize, c_int, c_int, c_int, c_int, u32) callconv(.c) c_int;
extern "user32" fn IsWindow(usize) callconv(.c) c_int;
extern "user32" fn IsWindowVisible(usize) callconv(.c) c_int;
extern "user32" fn GetDpiForWindow(usize) callconv(.c) u32;
extern "kernel32" fn GetCurrentProcessId() callconv(.c) u32;
extern "kernel32" fn SetLastError(u32) callconv(.c) void;
extern "kernel32" fn GetLastError() callconv(.c) u32;

pub fn valid(handle: usize) bool { return handle != 0 and IsWindow(handle) != 0; }

pub fn place(child: usize, logical_x: i32, logical_y: i32) !void {
    if (!owned(child)) return error.ForeignWindow;
    const dpi = GetDpiForWindow(child);
    if (dpi == 0) return error.InvalidDpi;
    if (SetWindowPos(child, 0, policy.physicalPosition(logical_x, dpi),
        policy.physicalPosition(logical_y, dpi), 0, 0, 0x15) == 0) return error.SetPositionFailed;
}

pub fn raiseControl(control: usize, topmost: bool) !void {
    if (!owned(control)) return error.ForeignWindow;
    const order: usize = if (topmost) std.math.maxInt(usize) else 0;
    if (SetWindowPos(control, order, 0, 0, 0, 0, 0x13) == 0) return error.SetOrderFailed;
}

fn owned(handle: usize) bool {
    var pid: u32 = 0;
    _ = GetWindowThreadProcessId(handle, &pid);
    return valid(handle) and pid == GetCurrentProcessId();
}

const Search = struct { title: []const u16, result: usize = 0 };
fn findOwnCallback(handle: usize, value: isize) callconv(.c) c_int {
    const search: *Search = @ptrFromInt(@as(usize, @bitCast(value)));
    if (!owned(handle)) return 1;
    var title: [256]u16 = undefined;
    const count = GetWindowTextW(handle, &title, title.len);
    if (count > 0 and std.mem.eql(u16, title[0..@intCast(count)], search.title)) {
        search.result = handle;
        return 0;
    }
    return 1;
}

pub fn ownWindow(comptime title: []const u8) usize {
    var search: Search = .{ .title = wide(title) };
    _ = EnumWindows(findOwnCallback, @bitCast(@intFromPtr(&search)));
    return search.result;
}

fn desktopCallback(handle: usize, value: isize) callconv(.c) c_int {
    const result: *usize = @ptrFromInt(@as(usize, @bitCast(value)));
    if (IsWindowVisible(handle) != 0 and FindWindowExW(handle, 0, wide("SHELLDLL_DefView"), null) != 0) {
        result.* = handle;
        return 0;
    }
    return 1;
}

pub fn desktopHost() !usize {
    var result: usize = 0;
    _ = EnumWindows(desktopCallback, @bitCast(@intFromPtr(&result)));
    if (result == 0) return error.DesktopHostUnavailable;
    return result;
}

fn getStyle(handle: usize, index: c_int) u32 {
    return @truncate(@as(usize, @bitCast(GetWindowLongPtrW(handle, index))));
}
fn setStyle(handle: usize, index: c_int, value: u32) !void {
    SetLastError(0);
    const previous = SetWindowLongPtrW(handle, index, @as(i32, @bitCast(value)));
    if (previous == 0 and GetLastError() != 0) return error.SetStyleFailed;
}

pub const Port = struct {
    pub fn capture(_: *Port, child: usize) !policy.Snapshot {
        if (!owned(child)) return error.ForeignWindow;
        var rect: Rect = undefined;
        if (GetWindowRect(child, &rect) == 0) return error.GetRectFailed;
        return .{
            .parent = GetParent(child), .style = getStyle(child, -16), .extended = getStyle(child, -20),
            .rect = .{ .x = rect.left, .y = rect.top, .width = rect.right - rect.left, .height = rect.bottom - rect.top },
        };
    }
    pub fn styles(_: *Port, child: usize, style: u32, extended: u32) !void {
        if (!owned(child)) return error.ForeignWindow;
        // SetWindowPos is required to update the real topmost state.
        if (SetWindowPos(child, std.math.maxInt(usize) - 1, 0, 0, 0, 0, 0x13) == 0) return error.SetOrderFailed;
        try setStyle(child, -16, style);
        try setStyle(child, -20, extended);
    }
    pub fn reparent(_: *Port, child: usize, parent: usize) !void {
        if (!owned(child) or !valid(parent)) return error.InvalidHandle;
        SetLastError(0);
        if (SetParent(child, parent) == 0 and GetLastError() != 0) return error.SetParentFailed;
        if (GetParent(child) != parent) return error.ParentMismatch;
    }
    pub fn position(_: *Port, child: usize, parent: usize, rect: policy.Rect) !void {
        var point: Point = .{ .x = rect.x, .y = rect.y };
        SetLastError(0);
        if (MapWindowPoints(0, parent, &point, 1) == 0 and GetLastError() != 0) return error.MapPointFailed;
        if (SetWindowPos(child, 0, point.x, point.y, rect.width, rect.height, 0x70) == 0) return error.SetPositionFailed;
    }
    pub fn restore(self: *Port, child: usize, old: policy.Snapshot) void {
        if (!owned(child)) return;
        _ = SetParent(child, old.parent);
        setStyle(child, -16, old.style) catch {};
        setStyle(child, -20, old.extended) catch {};
        self.position(child, old.parent, old.rect) catch {};
        const order: usize = if (old.extended & policy.topmost_style != 0) std.math.maxInt(usize) else std.math.maxInt(usize) - 1;
        _ = SetWindowPos(child, order, 0, 0, 0, 0, 0x13);
    }
};
