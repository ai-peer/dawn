enable f16;

fn modf_9997a3() {
  var arg_0 = vec2<f16>(f16());
  var res = modf(arg_0);
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
