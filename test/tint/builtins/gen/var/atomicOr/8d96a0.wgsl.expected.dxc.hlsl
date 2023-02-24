RWByteAddressBuffer sb_rw : register(u0, space0);

int tint_atomicOr(uint offset, int value) {
  int original_value = 0;
  sb_rw.InterlockedOr(offset, value, original_value);
  return original_value;
}


void atomicOr_8d96a0() {
  int arg_1 = 1;
  int res = tint_atomicOr(0u, arg_1);
}

void fragment_main() {
  atomicOr_8d96a0();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  atomicOr_8d96a0();
  return;
}
