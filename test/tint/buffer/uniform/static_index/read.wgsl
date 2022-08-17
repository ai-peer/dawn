enable f16;

struct Inner {
    @size(16) x : i32,
};

struct S {
    a : vec3<i32>,
    b : i32,
    c : vec3<u32>,
    d : u32,
    e : vec3<f32>,
    f : f32,
    g : vec3<f16>,
    h : f16,
    i : vec2<f16>,
    j : vec2<i32>,
    k : vec2<i32>,
    l : mat2x3<f32>,
    m : mat3x2<f32>,
    n : mat2x3<f16>,
    o : mat3x2<f16>,
    @align(16) p : Inner,
    @align(16) q : array<Inner, 4>,
};

@binding(0) @group(0) var<uniform> s : S;

@compute @workgroup_size(1)
fn main() {
    let a = s.a;
    let b = s.b;
    let c = s.c;
    let d = s.d;
    let e = s.e;
    let f = s.f;
    let g = s.g;
    let h = s.h;
    let i = s.i;
    let j = s.j;
    let k = s.k;
    let l = s.l;
    let m = s.m;
    let n = s.n;
    let o = s.o;
    let p = s.p;
    let q = s.q;
}
