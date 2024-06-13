enable chromium_experimental_subgroups;

fn subgroupBroadcast_c36fe1() -> u32 {
  var res : u32 = subgroupBroadcast(1u, 1u);
  return res;
}

@group(0) @binding(0) var<storage, read_write> prevent_dce : u32;

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupBroadcast_c36fe1();
}
