const std = @import("std");
const variant = @import("variant");
const samples = @import("samples");
const expectEqual = @import("std").testing.expectEqual;
const ArrayList = std.ArrayList;
const Allocator = std.mem.Allocator;
const p = @import("std").debug.print;

const hello = "Hello World from Zig";

export fn hello_zig2(msg: [*] const u8) [*]const u8 {
    const result = msg;
    return result;
}

pub fn VarWindow(comptime T: type) type {

    return struct {
        stack: ArrayList(T),

        const Self = @This();

        pub fn init(allocator: Allocator) Self {
            return Self{ .stack = ArrayList(T).init(allocator) };
        }

        pub fn deinit(self: *Self) void {
            self.stack.deinit();
        }

        pub fn push(self: *Self, val: T) !void {
            try self.stack.append(val);
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

const MyVarWindow = VarWindow(i32);

// Return a pointer to the Variant window class
// void *zig_variant_window();
export fn zig_variant_window() * MyVarWindow {

    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    // defer std.debug.assert(!gpa.deinit());

    var hello2 = MyVarWindow.init(gpa.allocator());
    hello2.push(12) catch unreachable;
    return &hello2;
}

export fn win_size(ptr: *MyVarWindow) usize {
    return ptr.count();
}

test "hello zig" {
    try expectEqual(hello_zig2(hello),hello);
}
