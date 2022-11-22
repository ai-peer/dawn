@compute @workgroup_size(1)
fn main() {
  let in = vec2(1.23, 3.45);
  let x = modf(in);
  let y = modf(vec2(1.23, 3.45));
}
