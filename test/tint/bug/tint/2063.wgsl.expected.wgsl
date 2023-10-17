var<workgroup> arg_0 : atomic<u32>;

@compute @workgroup_size(1)
fn compute_main() {
  let x = 1u;
  let y = 2u;
  var res : u32 = atomicSub(&(arg_0), (x + y));
}
