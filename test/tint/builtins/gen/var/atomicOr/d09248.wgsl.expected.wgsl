var<workgroup> arg_0 : atomic<i32>;

fn atomicOr_d09248() {
  var arg_1 = 1i;
  var res : i32 = atomicOr(&(arg_0), arg_1);
  prevent_dce = res;
}

@group(2) @binding(0) var<storage, read_write> prevent_dce : i32;

@compute @workgroup_size(1)
fn compute_main() {
  atomicOr_d09248();
}