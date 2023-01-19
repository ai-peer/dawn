// flags: --transform robustness

@group(0) @binding(0) var<storage, read_write> S : array<vec4f>;

fn f(i : i32, min : f32) {
  S[i] = vec4(1.0);
}
