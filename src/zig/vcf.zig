const std = @import("std");
// const variant = @import("variant");
const mem = @import("std").mem;
const fmt = std.fmt;
const samples = @import("samples");
const expectEqual = @import("std").testing.expectEqual;
const expect = @import("std").testing.expect;
const ArrayList = std.ArrayList;
const StringList = ArrayList([] const u8);
const Allocator = std.mem.Allocator;
const p = @import("std").debug.print;

const VCFError = error{
    UnexpectedOrder
};

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
        const buf: [*c]const u8 = var_id(self.v);
        const str = std.mem.span(@ptrCast([*:0]const u8, buf));
        return str;
    }

    pub fn pos(self: *Self) u64 {
        return var_pos(self.v);
    }

    pub fn ref(self: *Self) [:0] const u8 {
        const buf: [*c]const u8 = var_ref(self.v);
        const str = std.mem.span(@ptrCast([*:0]const u8, buf));
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


//    // set maxpos to the most outward allele position + its reference size
//    auto maxpos = first.position + first.ref.size();
//    for (auto v: vars) {
//         if (maxpos < v.position + v.ref.size()) {
//             maxpos = v.position + v.ref.size();
//         }
//     }

    const MockVariant = struct {
      id: [] const u8 = "TEST",
      pos: u64,
      ref: [] const u8,
      alt: ArrayList([] u8) = undefined
    };

fn refs_maxpos(list: std.ArrayList(MockVariant)) u64 {
    var mpos = list.items[0].pos;
    for (list.items) |v| {
            const npos = v.pos + v.ref.len;
            if (npos > mpos)
                mpos = npos;
        }
    return mpos;
}

/// Expands reference to overlap all variants
fn expand_ref(list: ArrayList(MockVariant)) !ArrayList(u8) {
    const allocator = std.testing.allocator;
    var res = ArrayList(u8).init(allocator);
    // defer res.deinit();
    // try res.append('T');
    const first = list.items[0];
    // concat2(&res,first.ref);
    try res.appendSlice(first.ref);
    // p("!{s}!",.{res});
    // defer test_allocator.free(result);

    const left0 = first.pos;
    for (list.items) |v| {
            const right0 = left0 + res.items.len;
            const left1 = v.pos;
            const right1 = left1 + v.ref.len;
            //            ref    sdiff
            // ref0     |AAAAA|------->|
            // ref1      |AAAAAAAAAAAAA|
            //           |--->| append |
            //            pdiff

            const sdiff = right1 - right0; // diff between ref0 and ref1 right positions
            const pdiff = right0 - left1; // diff between ref0 right and ref1 left
            if (sdiff >= 0) {
                // newref = ref + append
                try res.appendSlice(v.ref[pdiff..pdiff+sdiff]);
            }
            else {
                // We expect slices to move right
                return VCFError.UnexpectedOrder;
            }
            _ = pdiff;
        }
    return res;
}

fn expand_alt(ref: [] const u8, list: ArrayList(MockVariant)) !ArrayList(ArrayList(u8)) {
    _ = ref;
    const allocator = std.testing.allocator;
    // add alternates and splice them into the reference. It does not modify the ref.
    const first = list.items[0];
    var nalt = ArrayList(ArrayList(u8)).init(allocator);
    for (list.items) |v| {
            p("!{s}!\n",.{v.ref});
            const p5diff = v.pos - first.pos; // always >= 0 - will raise error otherwise
            const right0 = @intCast(i64, first.pos + first.ref.len);
            const right1 = @intCast(i64, v.pos + v.ref.len);
            const p3diff = right0 - right1; // can be < 0
            //            ref    sdiff
            // ref0     |AAAAA|------->|
            //        p5diff    p3diff
            // ref1      |AAAAAAAAAAAAA|
            //           |--->|        |
            //            pdiff
            const before = ref[0..p5diff]; // leading ref
            // if (p3diff > 0 && p3diff < mvar.ref.size()) {
            //   after = ref.substr(ref.size() - p3diff .. end);
            var after = ArrayList(u8).init(allocator);
            defer after.deinit();
            if (p3diff > 0 and p3diff < v.ref.len)
                try after.appendSlice(ref[ref.len-@intCast(u64,p3diff)..]);
            for (v.alt.items) | alt | {
                    p("alt={s}",.{alt});
                    var new = ArrayList(u8).init(allocator);
                    if (p3diff != 0 or p5diff != 0) {
                        try new.appendSlice(before);
                        try new.appendSlice(alt);
                        try new.appendSlice(after.items);
                        try nalt.append(new);
                    } else {
                        try new.appendSlice(alt);
                        try nalt.append(new);
                    }
            }
        }
    return nalt;
}

test "variant ref expansion" {

    // var c_var = var_parse("a\t281\t>1>9\tAGCCGGGGCAGAAAGTTCTTCCTTGAATGTGGTCATCTGCATTTCAGCTCAGGAATCCTGCAAAAGACAG\tCTGTCTTTTGCAGGATTCCTGTGCTGAAATGCAGATGACCGCATTCAAGGAAGAACTATCTGCCCCGGCT\t60.0\t.\tAC=1;AF=1;AN=1;AT=>1>2>3>4>5>6>7>8>9,>1<8>10<6>11<4>12<2>9;NS=1;LV=0\tGT\t1",false);
    // var v2 = Variant{.v = c_var};
    // p("---->{s}\n",.{v2.id()});
    // expect(mem.eql(u8, v2.id(), ">1>9")) catch |err| {
    //     std.debug.print("{e} <-> {s}\n", .{err,v2.id()});
    // };

    var list = std.ArrayList(MockVariant).init(std.testing.allocator);
    defer list.deinit();

    const v1 = MockVariant{ .pos = 10, .ref = "AAAA" };
    try expect(std.mem.eql(u8, v1.id, "TEST"));
    try list.append(v1);
    const v2 = MockVariant{ .pos = 10, .ref = "AAAAA" };
    try list.append(v2);
    const v3 = MockVariant{ .pos = 10, .ref = "AAAAACC" };
    try list.append(v3);
    const maxpos = refs_maxpos(list);
    p("<{any}>",.{maxpos});
    try expect(maxpos == 17);

    const nref = try expand_ref(list);
    // defer test_allocator.free(nref);
    // p("<{s}>",.{nref});
    // p("!{s}!",.{nref});
    try expect(nref.items.len == 7);
    try expect(std.mem.eql(u8, nref.items, "AAAAACC"));
    nref.deinit();
}

test "variant alt expansion" {
    var list = std.ArrayList(MockVariant).init(std.testing.allocator);
    defer {
        list.deinit();
    }

    var alt1 = std.ArrayList([] u8).init(std.testing.allocator);
    defer alt1.deinit();
    var a1 = [_]u8{'c', 'c'};
    try alt1.append(a1[0..]);
    const v1 = MockVariant{ .pos = 10, .ref = "AAAA", .alt = alt1 };
    try expect(std.mem.eql(u8, v1.id, "TEST"));
    try list.append(v1);

    var alt2 = std.ArrayList([] u8).init(std.testing.allocator);
    defer alt2.deinit();
    var a2 = [_]u8{'c', 'c', 'c'};
    try alt2.append(a2[0..]);
    const v2 = MockVariant{ .pos = 10, .ref = "AAAAA", .alt = alt2 };
    try list.append(v2);

    var alt3 = std.ArrayList([] u8).init(std.testing.allocator);
    defer alt3.deinit();
    var a3 = [_]u8{'c', 'c', 'c', 'c'};
    try alt3.append(a3[0..]);
    const v3 = MockVariant{ .pos = 10, .ref = "CC", .alt = alt3 };
    try list.append(v3);
    const nalt = try expand_alt("AAAAACC",list);
    defer {
        for (nalt.items) |item| {
                item.deinit();
            }
        nalt.deinit();
    }
    expect(nalt.items.len == 3) catch |e| {
        p("{e}: {d}",.{e,nalt.items.len});
        return;
    };
}

test {
    _ = @import("samples.zig");
}
