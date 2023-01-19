@compute @workgroup_size(1)
fn main() {
  const in = vec2(1.25, 3.75);
  let res = frexp(in);
  let fract_1 : vec2<f32> = res.fract;
  let exp_1 : vec2<i32> = res.exp;
}
