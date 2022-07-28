const std = @import("std");
// const variant = @import("variant");
const mem = @import("std").mem;
const samples = @import("samples");
const expectEqual = @import("std").testing.expectEqual;
const expect = @import("std").testing.expect;
const ArrayList = std.ArrayList;
const Allocator = std.mem.Allocator;
const p = @import("std").debug.print;

const hello = "Hello World from Zig";

// const Variant = @OpaqueType();

// C++ accessors for Variant object
pub extern fn var_id(* anyopaque) [*c] const u8;
extern fn var_pos(* anyopaque) u64;
extern fn var_ref(* anyopaque) [*c] const u8;
extern fn var_set_id(?* anyopaque, [*c] const u8) void;
extern fn call_c([*] const u8) void;

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
    // p("DONE {}\n",.{win_size()});
}

export fn win_push(win2: *MyVarWindow, vcfvar: *anyopaque) void {
    var ptr = @ptrToInt(vcfvar);
    _ = ptr;
    // var w: * MyVarWindow = @ptrCast(*MyVarWindow, @alignCast(@alignOf(*MyVarWindow), win2));
    var w = win2;
    // p("BEFORE:",.{});
    // p("use {p}\n",.{w});
    w.append(vcfvar) catch |err| {
                std.debug.print("out of memory {e}\n", .{err});
            };
    // p("AFTER\n",.{});
}

export fn win_size() usize {
    return win.items.len;
}


const Variant = struct {
    v: *anyopaque,

    const Self = @This();

    pub fn id(self: *Self) [:0]const u8 {
        const buffer: [*c]const u8 = var_id(self.v);
        const str = std.mem.span(@ptrCast([*:0]const u8, buffer));
        return str;
    }

    pub fn pos(self: *Self) u64 {
        return var_pos(self.v);
    }

    pub fn ref(self: *Self) [:0] const u8 {
        const buffer: [*c]const u8 = var_ref(self.v);
        const str = std.mem.span(@ptrCast([*:0]const u8, buffer));
        return str;
    }

};

// by @Cimport:
// pub extern fn zig_create_multi_allelic(retvar: ?*anyopaque, varlist: [*c]?*anyopaque, size: c_long) ?*anyopaque;

export fn zig_create_multi_allelic(variant: ?*anyopaque, varlist: [*c]?* anyopaque, size: usize) ?*anyopaque {
    _ = varlist;
    const c_str = var_id(variant.?);
    const s = @ptrCast([*c]const u8, c_str);
    p("And yes, we are back in zig: {s} -- {}\n\n",.{s,size});

    const p3 = @ptrCast(* anyopaque, varlist[3]);
    const s3 = var_id(p3);
    var v = Variant{.v = varlist[3].?};
    p("id={s} !{s}! pos={d} ref={s}\n",.{s3,v.id(),v.pos(),v.ref()});


    const as_slice: [:0]const u8 = std.mem.span(s3); // makes 0 terminated slice (sentinel value is zero byte)
    std.testing.expectEqualStrings(as_slice, ">3655>3662_4") catch |err| {
        std.debug.print("{e} {s}\n", .{err,as_slice});
    };

    // expectEqual(variant,@intToPtr(*anyopaque,varlist[0])) catch unreachable;
    // var vars = @ptrCast([*] u8, varlist);
    // Now walk the list
    var i:u64 = 0;
    for (varlist[0..size]) |ptr| {
             i = i + 1;
             const p2 = @ptrCast(* anyopaque, ptr);
             const s2 = var_id(p2);
             p("num = {}",.{i});
             p("id = {s}, pos = {d}\n",.{s2,var_pos(p2)});
         }
    // var_set_id(variant,"HELLO");
    return variant;
}

test "hello zig" {
    try expectEqual(hello_zig2(hello),hello);
}

test "variant" {
}

test {
    _ = @import("samples.zig");
}
