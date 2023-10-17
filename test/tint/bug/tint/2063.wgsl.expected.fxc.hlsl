groupshared uint arg_0;

struct tint_symbol_1 {
  uint local_invocation_index : SV_GroupIndex;
};

void compute_main_inner(uint local_invocation_index) {
  {
    uint atomic_result = 0u;
    InterlockedExchange(arg_0, 0u, atomic_result);
  }
  GroupMemoryBarrierWithGroupSync();
  const uint x = 1u;
  const uint y = 2u;
  uint atomic_result_1 = 0u;
  InterlockedAdd(arg_0, -(x + y), atomic_result_1);
  uint res = atomic_result_1;
}

[numthreads(1, 1, 1)]
void compute_main(tint_symbol_1 tint_symbol) {
  compute_main_inner(tint_symbol.local_invocation_index);
  return;
}
