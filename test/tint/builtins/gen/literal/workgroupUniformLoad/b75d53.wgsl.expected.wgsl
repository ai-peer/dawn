var<workgroup> arg_0 : bool;

fn workgroupUniformLoad_b75d53() {
  var res : bool = workgroupUniformLoad(&(arg_0));
}

@compute @workgroup_size(1)
fn compute_main() {
  workgroupUniformLoad_b75d53();
}
