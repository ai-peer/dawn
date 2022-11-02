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

void insertBits_87826b() {
  uint3 res = tint_insert_bits((1u).xxx, (1u).xxx, 1u, 1u);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  insertBits_87826b();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  insertBits_87826b();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  insertBits_87826b();
  return;
}
