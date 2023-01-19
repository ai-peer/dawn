@compute @workgroup_size(1)
fn main() {
  let in = 1.25;
  let res = frexp(in);
  let fract_1 : f32 = res.fract;
  let exp_1 : i32 = res.exp;
}
