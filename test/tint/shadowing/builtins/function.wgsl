// flags: --transform robustness

@group(0) @binding(0) var<storage, read_write> S : array<vec4f>;

fn min() {
}

fn f(i : i32) {
  min();
  S[i] = vec4(1.0);
}
