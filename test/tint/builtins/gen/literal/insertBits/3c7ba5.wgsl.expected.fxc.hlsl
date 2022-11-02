uint2 tint_insert_bits(uint2 v, uint2 n, uint offset, uint count) {
  uint2 result = v;
  if ((offset < 32u)) {
    const uint mask_low = ((1u << offset) - 1u);
    uint mask_high = 0u;
    if (((offset + count) >= 32u)) {
      mask_high = 4294967295u;
    } else {
      mask_high = ((1u << (offset + count)) - 1u);
    }
    const uint mask = (mask_low ^ mask_high);
    result = (((n << uint2((offset).xx)) & uint2((mask).xx)) | (v & uint2((~(mask)).xx)));
  }
  return result;
}

void insertBits_3c7ba5() {
  uint2 res = tint_insert_bits((1u).xx, (1u).xx, 1u, 1u);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  insertBits_3c7ba5();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  insertBits_3c7ba5();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  insertBits_3c7ba5();
  return;
}
