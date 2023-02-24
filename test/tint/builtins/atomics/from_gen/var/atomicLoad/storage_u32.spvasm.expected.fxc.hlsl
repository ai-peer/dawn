RWByteAddressBuffer sb_rw : register(u0, space0);

uint tint_atomicLoad(uint offset) {
  uint value = 0;
  sb_rw.InterlockedOr(offset, 0, value);
  return value;
}


void atomicLoad_fe6cc3() {
  uint res = 0u;
  const uint x_9 = tint_atomicLoad(0u);
  res = x_9;
  return;
}

void fragment_main_1() {
  atomicLoad_fe6cc3();
  return;
}

void fragment_main() {
  fragment_main_1();
  return;
}

void compute_main_1() {
  atomicLoad_fe6cc3();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  compute_main_1();
  return;
}
