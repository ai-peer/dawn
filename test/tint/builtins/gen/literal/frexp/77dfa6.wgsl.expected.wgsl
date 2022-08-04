enable f16;

fn frexp_77dfa6() {
  var res = frexp(vec4<f16>(f16()));
}

@vertex
fn vertex_main() -> @builtin(position) vec4<f32> {
  frexp_77dfa6();
  return vec4<f32>();
}

@fragment
fn fragment_main() {
  frexp_77dfa6();
}

@compute @workgroup_size(1)
fn compute_main() {
  frexp_77dfa6();
}
