Texture2D tex : register(t0, space0);
RWByteAddressBuffer result : register(u1, space0);

struct tint_symbol_2 {
  uint3 GlobalInvocationId : SV_DispatchThreadID;
};

void main_inner(uint3 GlobalInvocationId) {
  const uint tint_symbol = ((GlobalInvocationId.y * 128u) + GlobalInvocationId.x);
  result.Store((4u * tint_symbol), asuint(tex.Load(int3(int(GlobalInvocationId.x), int(GlobalInvocationId.y), 0)).x));
}

[numthreads(1, 1, 1)]
void main(tint_symbol_2 tint_symbol_1) {
  main_inner(tint_symbol_1.GlobalInvocationId);
  return;
}
