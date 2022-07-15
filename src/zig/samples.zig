const std = @import("std");
const ArrayList = std.ArrayList;
const expect = @import("std").testing.expect;
const expectEqual = @import("std").testing.expectEqual;
const p = @import("std").debug.print;

const hello = "Hello World from Zig";

export fn hello_zig(msg: [*] const u8) [*]const u8 {
    const result = msg;
    return result;
}

export fn zig_process_vector(vsize: u64, v: [*][*] const u8) void {
    // std.debug.print("{s}",v[0]);
    const s = v[0];
    const s1 = v[1];
    // std.testing.expectEqual(s, "1/0") catch unreachable;
    _ = s;
    p("HELLO! {any} {c}:{c}\n",.{vsize, s[0],s1[2]}); // expect 1 and dot
    // return false;
}

fn split_genotypes(str: []const u8) *ArrayList([] const u8) {
    const test_allocator = std.testing.allocator;

    var list = ArrayList([] const u8).init(test_allocator);
    defer list.deinit();
    var splits = std.mem.split(u8, str, " ");
    while (splits.next()) |chunk| {
            list.append(chunk) catch |err| {
                std.debug.print("out of memory {e}\n", .{err});
            };
        }
    return &list;
}



test "hello zig" {
    try expectEqual(hello_zig(hello),hello);
}

test "split genotypes" {
    const input_genotypes = "1|0 .|1 0|1 1|1";
    try std.testing.expectEqual(split_genotypes(input_genotypes).items.len, 4);
}
