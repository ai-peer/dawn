enable f16;

fn frexp_7048e3() {
  var arg_0 = f16();
  var res = frexp(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  frexp_7048e3();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  frexp_7048e3();
}

@compute @workgroup_size(1)
fn compute_main() {
  frexp_7048e3();
}
