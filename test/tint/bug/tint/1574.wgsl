@group(0) @binding(0)
var<storage, read_write> a : atomic<u32>;

@stage(compute) @workgroup_size(16)
fn main() {
  var value = 42u;
  let r1 = atomicCompareExchangeWeak(&a, 0u, value);
  let r2 = atomicCompareExchangeWeak(&a, 0u, value);
  let r3 = atomicCompareExchangeWeak(&a, 0u, value);
}

