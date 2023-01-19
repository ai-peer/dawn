@compute @workgroup_size(1)
fn main() {
  let res = frexp(1.23);
  let exp_1 : i32 = res.exp;
  let fract_1 : f32 = res.fract;
}
