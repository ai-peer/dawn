enable chromium_experimental_subgroups;

fn subgroupBallot_7e6d0e() -> vec4<u32> {
  var res : vec4<u32> = subgroupBallot();
  return res;
}

@group(0) @binding(0) var<storage, read_write> prevent_dce : vec4<u32>;

@compute @workgroup_size(1)
fn compute_main() {
  prevent_dce = subgroupBallot_7e6d0e();
}
