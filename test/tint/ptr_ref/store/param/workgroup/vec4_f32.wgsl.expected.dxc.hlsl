groupshared float4 S;

void func_S() {
  S = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

struct tint_symbol_1 {
  uint local_invocation_index : SV_GroupIndex;
};

void main_inner(uint local_invocation_index) {
  {
    S = float4(0.0f, 0.0f, 0.0f, 0.0f);
  }
  GroupMemoryBarrierWithGroupSync();
  func_S();
}

[numthreads(1, 1, 1)]
void main(tint_symbol_1 tint_symbol) {
  main_inner(tint_symbol.local_invocation_index);
  return;
}
