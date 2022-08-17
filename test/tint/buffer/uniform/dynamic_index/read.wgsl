enable f16;

struct Inner {
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
    @align(16) p : mat3x2<f32>,
    @align(16) q : array<vec4<i32>, 4>,
};

struct S {
    arr : array<Inner, 8>,
};

@binding(0) @group(0) var<uniform> s : S;

@compute @workgroup_size(1)
fn main(@builtin(local_invocation_index) idx : u32) {
    let a = s.arr[idx].a;
    let b = s.arr[idx].b;
    let c = s.arr[idx].c;
    let d = s.arr[idx].d;
    let e = s.arr[idx].e;
    let f = s.arr[idx].f;
    let g = s.arr[idx].g;
    let h = s.arr[idx].h;
    let i = s.arr[idx].i;
    let j = s.arr[idx].j;
    let k = s.arr[idx].k;
    let l = s.arr[idx].l;
    let m = s.arr[idx].m;
    let n = s.arr[idx].n;
    let o = s.arr[idx].o;
    let p = s.arr[idx].p;
    let q = s.arr[idx].q;
}
