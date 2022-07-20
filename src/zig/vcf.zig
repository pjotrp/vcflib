const std = @import("std");
const variant = @import("variant");
const samples = @import("samples");
const expectEqual = @import("std").testing.expectEqual;

const hello = "Hello World from Zig";

export fn hello_zig2(msg: [*] const u8) [*]const u8 {
    const result = msg;
    return result;
}

test "hello zig" {
try expectEqual(hello_zig2(hello),hello);
}
