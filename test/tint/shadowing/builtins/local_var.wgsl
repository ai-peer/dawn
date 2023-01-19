// flags: --transform robustness

@group(0) @binding(0) var<storage, read_write> S : array<vec4f>;

fn f(i : i32) {
  var min = 42;
  S[i] = vec4(1.0);
}
