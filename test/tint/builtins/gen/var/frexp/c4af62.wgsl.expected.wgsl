enable f16;

fn frexp_c4af62() {
  var arg_0 = vec3<f16>(f16());
  var res = frexp(arg_0);
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  frexp_c4af62();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  frexp_c4af62();
}

@compute @workgroup_size(1)
fn compute_main() {
  frexp_c4af62();
}
