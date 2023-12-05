@group(0) @binding(0) var<storage, read_write> s: i32;

var<workgroup> g1 : atomic<i32>;

fn d(val: i32) -> i32 {
  return val;
}

fn c(val: ptr<function, i32>) -> i32 {
  return *val + d(*val);
}

fn a(val: ptr<function, i32>) -> i32 {
  return *val + c(val);
}

fn z() -> i32 {
  return atomicLoad(&g1);
}

fn y (v1: ptr<function, vec3f>) -> i32 {
  (*v1).x = cross(*v1, *v1).x;
  return i32((*v1).x);
}

struct S {
  a: i32,
  b: i32,
}

fn b(val: ptr<function, S>) -> i32 {
  return (*val).a + (*val).b;
}

@compute @workgroup_size(1)
fn main() {
  var v1 = 0;
  var v2 = S();
  let v3 = &v2;
  var v4 = vec3f();
  let t1 = atomicLoad(&g1);

  s = a(&v1) + b(&v2) + b(v3) + z() + t1 + y(&v4);
}
