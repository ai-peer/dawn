int4 tint_insert_bits(int4 v, int4 n, uint offset, uint count) {
  int4 result = v;
  if ((offset < 32u)) {
    const uint mask_low = ((1u << offset) - 1u);
    uint mask_high = 0u;
    if (((offset + count) >= 32u)) {
      mask_high = 4294967295u;
    } else {
      mask_high = ((1u << (offset + count)) - 1u);
    }
    const uint mask = (mask_low ^ mask_high);
    result = (((n << uint4((offset).xxxx)) & int4((int(mask)).xxxx)) | (v & int4((int(~(mask))).xxxx)));
  }
  return result;
}

void insertBits_d86978() {
  int4 arg_0 = (1).xxxx;
  int4 arg_1 = (1).xxxx;
  uint arg_2 = 1u;
  uint arg_3 = 1u;
  int4 res = tint_insert_bits(arg_0, arg_1, arg_2, arg_3);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  insertBits_d86978();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  insertBits_d86978();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  insertBits_d86978();
  return;
}
