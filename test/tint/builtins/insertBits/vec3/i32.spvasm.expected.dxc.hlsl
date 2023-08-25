int3 tint_insert_bits(int3 v, int3 n, uint offset, uint count) {
  const uint e = (offset + count);
  const uint mask = ((((offset < 32u) ? (1u << offset) : 0u) - 1u) ^ (((e < 32u) ? (1u << e) : 0u) - 1u));
  return ((((offset < 32u) ? (n << uint3((offset).xxx)) : int3(0, 0, 0)) & int3((int(mask)).xxx)) | (v & int3((int(~(mask))).xxx)));
}

void f_1() {
  int3 v = int3(0, 0, 0);
  int3 n = int3(0, 0, 0);
  uint offset_1 = 0u;
  uint count = 0u;
  const int3 x_16 = tint_insert_bits(v, n, offset_1, count);
  return;
}

[numthreads(1, 1, 1)]
void f() {
  f_1();
  return;
}
