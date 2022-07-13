const std = @import("std");
const ArrayList = std.ArrayList;
const expect = @import("std").testing.expect;
const expectEqual = @import("std").testing.expectEqual;

const hello = "Hello World from Zig";

export fn hello_zig() [*]const u8 {
    const result = hello;
    return result;
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
    try expectEqual(hello_zig(),hello);
}

test "split genotypes" {
    const input_genotypes = "1|0 .|1 0|1 1|1";
    try std.testing.expectEqual(split_genotypes(input_genotypes).items.len, 4);
}
