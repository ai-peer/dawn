RWByteAddressBuffer sb_rw : register(u0, space0);

uint tint_atomicMin(uint offset, uint value) {
  uint original_value = 0;
  sb_rw.InterlockedMin(offset, value, original_value);
  return original_value;
}


void atomicMin_c67a74() {
  uint arg_1 = 1u;
  uint res = tint_atomicMin(0u, arg_1);
}

void fragment_main() {
  atomicMin_c67a74();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  atomicMin_c67a74();
  return;
}
