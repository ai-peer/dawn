RWByteAddressBuffer sb_rw : register(u0, space0);

uint tint_atomicMax(uint offset, uint value) {
  uint original_value = 0;
  sb_rw.InterlockedMax(offset, value, original_value);
  return original_value;
}


void atomicMax_51b9be() {
  uint res = 0u;
  const uint x_9 = tint_atomicMax(0u, 1u);
  res = x_9;
  return;
}

void fragment_main_1() {
  atomicMax_51b9be();
  return;
}

void fragment_main() {
  fragment_main_1();
  return;
}

void compute_main_1() {
  atomicMax_51b9be();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  compute_main_1();
  return;
}
