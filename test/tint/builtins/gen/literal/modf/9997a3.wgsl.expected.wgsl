enable f16;

fn modf_9997a3() {
  var res = modf(vec2<f16>(f16()));
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  modf_9997a3();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  modf_9997a3();
}

@compute @workgroup_size(1)
fn compute_main() {
  modf_9997a3();
}
