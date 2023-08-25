int3 tint_extract_bits(int3 v, uint offset, uint count) {
  const uint s = min(offset, 32u);
  const uint e = min(32u, (s + count));
  const uint shl = (32u - e);
  const uint shr = (shl + s);
  const int3 shl_result = ((shl < 32u) ? (v << uint3((shl).xxx)) : int3(0, 0, 0));
  return ((shr < 32u) ? (shl_result >> uint3((shr).xxx)) : ((shl_result >> uint3(31u, 31u, 31u)) >> uint3(1u, 1u, 1u)));
}

RWByteAddressBuffer prevent_dce : register(u0, space2);

void extractBits_e04f5d() {
  int3 arg_0 = int3(1, 1, 1);
  uint arg_1 = 1u;
  uint arg_2 = 1u;
  int3 res = tint_extract_bits(arg_0, arg_1, arg_2);
  prevent_dce.Store3(0u, asuint(res));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  extractBits_e04f5d();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  extractBits_e04f5d();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  extractBits_e04f5d();
  return;
}
