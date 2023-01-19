@compute @workgroup_size(1)
fn main() {
  const const_in = 1.25;
  let runtime_in = 1.25;
  var res = frexp(const_in);
  res = frexp(runtime_in);
  res = frexp(const_in);
  let fract_1 : f32 = res.fract;
  let exp_1 : i32 = res.exp;
}
