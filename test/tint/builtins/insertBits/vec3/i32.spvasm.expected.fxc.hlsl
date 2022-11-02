int3 tint_insert_bits(int3 v, int3 n, uint offset, uint count) {
  int3 result = v;
  if ((offset < 32u)) {
    const uint mask_low = ((1u << offset) - 1u);
    uint mask_high = 0u;
    if (((offset + count) >= 32u)) {
      mask_high = 4294967295u;
    } else {
      mask_high = ((1u << (offset + count)) - 1u);
    }
    const uint mask = (mask_low ^ mask_high);
    result = (((n << uint3((offset).xxx)) & int3((int(mask)).xxx)) | (v & int3((int(~(mask))).xxx)));
  }
  return result;
}

void f_1() {
  int3 v = (0).xxx;
  int3 n = (0).xxx;
  uint offset_1 = 0u;
  uint count = 0u;
  const int3 x_18 = v;
  const int3 x_19 = n;
  const uint x_20 = offset_1;
  const uint x_21 = count;
  const int3 x_16 = tint_insert_bits(x_18, x_19, x_20, x_21);
  return;
}

[numthreads(1, 1, 1)]
void f() {
  f_1();
  return;
}
