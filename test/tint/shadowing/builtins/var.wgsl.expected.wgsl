@group(0) @binding(0) var<storage, read_write> S : array<vec4f>;

var<private> min_1 = 42;

fn f(i : i32) {
  _ = min_1;
  S[min(u32(i), (arrayLength(&(S)) - 1u))] = vec4(1.0);
}
