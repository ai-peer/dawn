@group(0) @binding(0) var<storage, read_write> S : array<vec4f>;

fn f(i : i32, min_1 : f32) {
  S[min(u32(i), (arrayLength(&(S)) - 1u))] = vec4(1.0);
}
