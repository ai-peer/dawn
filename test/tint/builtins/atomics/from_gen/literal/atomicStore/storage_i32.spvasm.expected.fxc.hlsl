RWByteAddressBuffer sb_rw : register(u0, space0);

void tint_atomicStore(uint offset, int value) {
  int ignored;
  sb_rw.InterlockedExchange(offset, value, ignored);
}


void atomicStore_d1e9a6() {
  tint_atomicStore(0u, 1);
  return;
}

void fragment_main_1() {
  atomicStore_d1e9a6();
  return;
}

void fragment_main() {
  fragment_main_1();
  return;
}

void compute_main_1() {
  atomicStore_d1e9a6();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  compute_main_1();
  return;
}
