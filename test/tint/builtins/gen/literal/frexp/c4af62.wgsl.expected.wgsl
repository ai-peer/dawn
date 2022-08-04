enable f16;

fn frexp_c4af62() {
  var res = frexp(vec3<f16>(f16()));
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
