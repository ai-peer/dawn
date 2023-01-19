// flags: --transform robustness

@group(0) @binding(0) var<storage, read_write> S : array<vec4f>;

var<private> min = 42;

fn f(i : i32) {
  _ = min;
  S[i] = vec4(1.0);
}
