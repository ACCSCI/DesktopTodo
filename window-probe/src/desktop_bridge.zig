//! Decorates the SDK event boundary; reducer and view remain OS-independent.
const std = @import("std");
const sdk = @import("native_sdk");
const main = @import("main.zig");
const win = @import("desktop_win32.zig");
const policy = @import("window_mode.zig");

pub const Bridge = struct {
    inner: sdk.App,
    ui: *main.CounterApp,
    parent: usize = 0,
    note: usize = 0,
    control: usize = 0,
    reported_error: bool = false,
    reconciling: bool = false,
    mode: ?policy.Mode = null,

    pub fn app(self: *Bridge) sdk.App {
        var result = self.inner;
        result.context = self;
        result.start_fn = if (self.inner.start_fn != null) start else null;
        result.event_fn = event;
        result.source_fn = if (self.inner.source_fn != null) source else null;
        result.scene_fn = if (self.inner.scene_fn != null) scene else null;
        result.stop_fn = if (self.inner.stop_fn != null) stop else null;
        result.replay_fn = if (self.inner.replay_fn != null) replay else null;
        return result;
    }
    fn from(context: *anyopaque) *Bridge { return @ptrCast(@alignCast(context)); }
    fn start(context: *anyopaque, runtime: *sdk.Runtime) anyerror!void {
        try from(context).inner.start(runtime);
    }
    fn source(context: *anyopaque) anyerror!sdk.platform.WebViewSource {
        return from(context).inner.webViewSource();
    }
    fn scene(context: *anyopaque) anyerror!sdk.ShellConfig {
        return (try from(context).inner.scene()) orelse .{};
    }
    fn stop(context: *anyopaque, runtime: *sdk.Runtime) anyerror!void {
        try from(context).inner.stop(runtime);
    }
    fn replay(context: *anyopaque, control: sdk.runtime.ReplayControl) anyerror!void {
        try from(context).inner.replayControl(control);
    }
    fn event(context: *anyopaque, runtime: *sdk.Runtime, value: sdk.Event) anyerror!void {
        const self = from(context);
        self.inner.event(runtime, value) catch |err| {
            std.log.err("desktop delegate event {t} failed: {t}", .{ value, err });
            return err;
        };
        if (@import("builtin").os.tag == .windows and !@import("builtin").is_test) {
            self.reconcile() catch |err| {
                if (!self.reported_error) std.log.err("desktop attachment failed: {t}", .{err});
                self.reported_error = true;
            };
        }
    }
    fn reconcile(self: *Bridge) !void {
        // SetParent/SetWindowPos synchronously emit native window events.
        // They must not re-enter the same attachment transaction.
        if (self.reconciling) return;
        self.reconciling = true;
        defer self.reconciling = false;
        if (!self.ui.model.overlay_open) {
            self.note = 0;
            self.control = 0;
            self.reported_error = false;
            return;
        }
        if (self.mode != self.ui.model.mode) {
            self.note = 0;
            self.control = 0;
            self.mode = self.ui.model.mode;
        }
        const old_note = self.note;
        const old_control = self.control;
        if (self.ui.model.mode != .desktop) {
            if (!win.valid(self.note)) {
                self.note = win.ownWindow("Todo Overlay Probe");
                if (self.note != 0) try win.place(self.note, 540, 120);
            }
            if (!win.valid(self.control)) {
                self.control = win.ownWindow("Todo Unlock Probe");
                if (self.control != 0) try win.place(self.control, 750, 120);
            }
            if (policy.controlsNeedRaise(old_note, old_control, self.note, self.control))
                try win.raiseControl(self.control, self.ui.model.mode == .topmost);
            return;
        }
        if (!win.valid(self.parent)) self.parent = try win.desktopHost();
        var port: win.Port = .{};
        if (!win.valid(self.note)) {
            const note = win.ownWindow("Todo Overlay Probe");
            if (note != 0) {
                try win.place(note, 540, 120);
                try policy.attach(&port, note, self.parent);
                self.note = note;
                std.log.info("desktop attached note={d} parent={d}", .{ note, self.parent });
            }
        }
        if (!win.valid(self.control)) {
            const control = win.ownWindow("Todo Unlock Probe");
            if (control != 0) {
                try win.place(control, 750, 120);
                try policy.attach(&port, control, self.parent);
                self.control = control;
                std.log.info("desktop attached control={d} parent={d}", .{ control, self.parent });
            }
        }
        if (policy.controlsNeedRaise(old_note, old_control, self.note, self.control))
            try win.raiseControl(self.control, false);
    }
};
