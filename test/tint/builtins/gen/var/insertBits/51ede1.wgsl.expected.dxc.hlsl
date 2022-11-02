uint4 tint_insert_bits(uint4 v, uint4 n, uint offset, uint count) {
  uint4 result = v;
  if ((offset < 32u)) {
    const uint mask_low = ((1u << offset) - 1u);
    uint mask_high = 0u;
    if (((offset + count) >= 32u)) {
      mask_high = 4294967295u;
    } else {
      mask_high = ((1u << (offset + count)) - 1u);
    }
    const uint mask = (mask_low ^ mask_high);
    result = (((n << uint4((offset).xxxx)) & uint4((mask).xxxx)) | (v & uint4((~(mask)).xxxx)));
  }
  return result;
}

void insertBits_51ede1() {
  uint4 arg_0 = (1u).xxxx;
  uint4 arg_1 = (1u).xxxx;
  uint arg_2 = 1u;
  uint arg_3 = 1u;
  uint4 res = tint_insert_bits(arg_0, arg_1, arg_2, arg_3);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  insertBits_51ede1();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  insertBits_51ede1();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  insertBits_51ede1();
  return;
}
