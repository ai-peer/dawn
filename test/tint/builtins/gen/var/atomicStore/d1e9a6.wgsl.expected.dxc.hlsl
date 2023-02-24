RWByteAddressBuffer sb_rw : register(u0, space0);

void tint_atomicStore(uint offset, int value) {
  int ignored;
  sb_rw.InterlockedExchange(offset, value, ignored);
}


void atomicStore_d1e9a6() {
  int arg_1 = 1;
  tint_atomicStore(0u, arg_1);
}

void fragment_main() {
  atomicStore_d1e9a6();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  atomicStore_d1e9a6();
  return;
}
