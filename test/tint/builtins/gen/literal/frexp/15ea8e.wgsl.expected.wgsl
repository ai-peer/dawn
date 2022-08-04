enable f16;

fn frexp_15ea8e() {
  var res = frexp(vec2<f16>(f16()));
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  frexp_15ea8e();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  frexp_15ea8e();
}

@compute @workgroup_size(1)
fn compute_main() {
  frexp_15ea8e();
}
