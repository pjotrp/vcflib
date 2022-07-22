const std = @import("std");
const variant = @import("variant");
const samples = @import("samples");
const expectEqual = @import("std").testing.expectEqual;
const ArrayList = std.ArrayList;
const Allocator = std.mem.Allocator;
const p = @import("std").debug.print;

const hello = "Hello World from Zig";

// const Variant = @OpaqueType();

export fn hello_zig2(msg: [*] const u8) [*]const u8 {
    const result = msg;
    return result;
}

pub fn VarWindowX(comptime T: type) type {

    return struct {
        stack: ArrayList(T),

        const Self = @This();

        pub fn init(allocator: Allocator) Self {
            return Self{ .stack = ArrayList(T).init(allocator) };
        }

        pub fn deinit(self: *Self) void {
            self.stack.deinit();
        }

        pub fn push(self: *Self, value: T) !void {
            p("HELLO",.{});
            try self.stack.append(value);
            p("HELLOEXIT",.{});
        }

        pub fn pop(self: *Self) ?T {
            return self.stack.popOrNull();
        }

        pub fn top(self: *Self) ?T {
            if (self.stack.items.len == 0) {
                return null;
            }
            return self.stack.items[self.stack.items.len - 1];
        }

        pub fn count(self: *Self) usize {
            return self.stack.items.len;
        }

        pub fn isEmpty(self: *Self) bool {
            return self.count() == 0;
        }
    };
}

const test_allocator = std.testing.allocator;
const MyVarWindow = ArrayList(* anyopaque);
var win = MyVarWindow.init(test_allocator);

// Return a pointer to the Variant window class
// void *zig_variant_window();
export fn zig_variant_window() * anyopaque {
    return &win;
}

export fn zig_variant_window_cleanup(win2: *MyVarWindow) void {
    _ = win2;
    p("DONE {}\n",.{win_size()});
}

export fn win_push(win2: *MyVarWindow, vcfvar: *anyopaque) void {
    var ptr = @ptrToInt(vcfvar);
    _ = ptr;
    // var w: * MyVarWindow = @ptrCast(*MyVarWindow, @alignCast(@alignOf(*MyVarWindow), win2));
    var w = win2;
    p("BEFORE:",.{});
    p("use {p}\n",.{w});
    w.append(vcfvar) catch |err| {
                std.debug.print("out of memory {e}\n", .{err});
            };
    p("AFTER\n",.{});
}

export fn win_size() usize {
    return win.items.len;
}

test "hello zig" {
    try expectEqual(hello_zig2(hello),hello);
}
