@compute @workgroup_size(1)
fn main() {
  let in = 1.23;
  let x = modf(in);
  let y = modf(1.23);
}
