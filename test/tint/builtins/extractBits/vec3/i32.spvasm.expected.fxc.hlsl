int3 tint_extract_bits(int3 v, uint offset, uint count) {
  const uint s = min(offset, 32u);
  const uint e = min(32u, (s + count));
  const uint shl = (32u - e);
  const uint shr = (shl + s);
  const int3 shl_result = ((shl < 32u) ? (v << uint3((shl).xxx)) : int3(0, 0, 0));
  return ((shr < 32u) ? (shl_result >> uint3((shr).xxx)) : ((shl_result >> uint3(31u, 31u, 31u)) >> uint3(1u, 1u, 1u)));
}

void f_1() {
  int3 v = int3(0, 0, 0);
  uint offset_1 = 0u;
  uint count = 0u;
  const int3 x_15 = tint_extract_bits(v, offset_1, count);
  return;
}

[numthreads(1, 1, 1)]
void f() {
  f_1();
  return;
}
