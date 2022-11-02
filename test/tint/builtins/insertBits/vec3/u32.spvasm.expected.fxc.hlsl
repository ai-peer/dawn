uint3 tint_insert_bits(uint3 v, uint3 n, uint offset, uint count) {
  uint3 result = v;
  if ((offset < 32u)) {
    const uint mask_low = ((1u << offset) - 1u);
    uint mask_high = 0u;
    if (((offset + count) >= 32u)) {
      mask_high = 4294967295u;
    } else {
      mask_high = ((1u << (offset + count)) - 1u);
    }
    const uint mask = (mask_low ^ mask_high);
    result = (((n << uint3((offset).xxx)) & uint3((mask).xxx)) | (v & uint3((~(mask)).xxx)));
  }
  return result;
}

void f_1() {
  uint3 v = (0u).xxx;
  uint3 n = (0u).xxx;
  uint offset_1 = 0u;
  uint count = 0u;
  const uint3 x_17 = v;
  const uint3 x_18 = n;
  const uint x_19 = offset_1;
  const uint x_20 = count;
  const uint3 x_15 = tint_insert_bits(x_17, x_18, x_19, x_20);
  return;
}

[numthreads(1, 1, 1)]
void f() {
  f_1();
  return;
}
