pub const Mode = enum {
    desktop,
    topmost,
    window,

    pub fn bodyLabel(mode: Mode, locked: bool) []const u8 {
        return switch (mode) {
            .desktop => if (locked) "desktop-locked" else "desktop-unlocked",
            .topmost => if (locked) "note-locked" else "note-unlocked",
            .window => if (locked) "window-locked" else "window-unlocked",
        };
    }

    pub fn controlLabel(mode: Mode) []const u8 {
        return switch (mode) {
            .desktop => "desktop-unlock-control",
            .topmost => "unlock-control",
            .window => "window-unlock-control",
        };
    }
};

pub const Rect = struct { x: i32, y: i32, width: i32, height: i32 };
pub const Snapshot = struct { parent: usize, style: u32, extended: u32, rect: Rect };
pub const child_style: u32 = 0x40000000;
pub const popup_style: u32 = 0x80000000;
pub const topmost_style: u32 = 0x00000008;
pub const appwindow_style: u32 = 0x00040000;
pub const toolwindow_style: u32 = 0x00000080;

pub fn controlsNeedRaise(old_note: usize, old_control: usize, note: usize, control: usize) bool {
    return note != 0 and control != 0 and (old_note != note or old_control != control);
}

pub fn physicalPosition(logical: i32, dpi: u32) i32 {
    return @intCast(@divTrunc(@as(i64, logical) * dpi, 96));
}

/// An OS-independent transaction. Failed attachment restores the original
/// parent, styles and screen geometry; the app never owns Explorer windows.
pub fn attach(port: anytype, child: usize, parent: usize) !void {
    if (child == 0 or parent == 0 or child == parent) return error.InvalidHandle;
    const old = try port.capture(child);
    errdefer port.restore(child, old);
    try port.styles(child,
        (old.style & ~popup_style) | child_style,
        (old.extended & ~(topmost_style | appwindow_style)) | toolwindow_style);
    try port.reparent(child, parent);
    try port.position(child, parent, old.rect);
}
