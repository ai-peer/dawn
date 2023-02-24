RWByteAddressBuffer sb_rw : register(u0, space0);

int tint_atomicMin(uint offset, int value) {
  int original_value = 0;
  sb_rw.InterlockedMin(offset, value, original_value);
  return original_value;
}


void atomicMin_8e38dc() {
  int res = tint_atomicMin(0u, 1);
}

void fragment_main() {
  atomicMin_8e38dc();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  atomicMin_8e38dc();
  return;
}
