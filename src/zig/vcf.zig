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

// C++ constructor
extern fn var_parse(line: [*c] const u8, parse_samples: bool) *anyopaque;

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

const test_allocator = std.testing.allocator;

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
    var v1 = var_parse("TEST\t1\t2\t3\t4\tt5\t6",false);
    _ = v1;
    var c_var = var_parse("a\t281\t>1>9\tAGCCGGGGCAGAAAGTTCTTCCTTGAATGTGGTCATCTGCATTTCAGCTCAGGAATCCTGCAAAAGACAG\tCTGTCTTTTGCAGGATTCCTGTGCTGAAATGCAGATGACCGCATTCAAGGAAGAACTATCTGCCCCGGCT\t60.0\t.\tAC=1;AF=1;AN=1;AT=>1>2>3>4>5>6>7>8>9,>1<8>10<6>11<4>12<2>9;NS=1;LV=0\tGT\t1",false);
    var v2 = Variant{.v = c_var};
    p("---->{s}\n",.{v2.id().ptr});
    expect(mem.eql(u8, v2.id(), ">1>9")) catch unreachable;
    expectEqual(v2.id().len,">1>9".len) catch |err| {
        std.debug.print("{e} <-> {s}\n", .{err,v2.id()});
    };
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
    // var c_var = var_parse("a\t281\t>1>9\tAGCCGGGGCAGAAAGTTCTTCCTTGAATGTGGTCATCTGCATTTCAGCTCAGGAATCCTGCAAAAGACAG\tCTGTCTTTTGCAGGATTCCTGTGCTGAAATGCAGATGACCGCATTCAAGGAAGAACTATCTGCCCCGGCT\t60.0\t.\tAC=1;AF=1;AN=1;AT=>1>2>3>4>5>6>7>8>9,>1<8>10<6>11<4>12<2>9;NS=1;LV=0\tGT\t1",false);
    // var v2 = Variant{.v = c_var};
    // p("---->{s}\n",.{v2.id()});
    // expect(mem.eql(u8, v2.id(), ">1>9")) catch |err| {
    //     std.debug.print("{e} <-> {s}\n", .{err,v2.id()});
    // };

}

test {
    _ = @import("samples.zig");
}
