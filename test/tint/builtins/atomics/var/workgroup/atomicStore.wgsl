var<workgroup> wg: atomic<u32>;

@compute @workgroup_size(1)
fn compute_main() {
  atomicStore(&wg, 1u);
}
