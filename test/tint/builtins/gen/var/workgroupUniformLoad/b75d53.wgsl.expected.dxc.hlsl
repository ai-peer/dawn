groupshared bool arg_0;

bool tint_workgroupUniformLoad_arg_0() {
  GroupMemoryBarrierWithGroupSync();
  const bool result = arg_0;
  GroupMemoryBarrierWithGroupSync();
  return result;
}

void workgroupUniformLoad_b75d53() {
  bool res = tint_workgroupUniformLoad_arg_0();
}

struct tint_symbol_1 {
  uint local_invocation_index : SV_GroupIndex;
};

void compute_main_inner(uint local_invocation_index) {
  {
    arg_0 = false;
  }
  GroupMemoryBarrierWithGroupSync();
  workgroupUniformLoad_b75d53();
}

[numthreads(1, 1, 1)]
void compute_main(tint_symbol_1 tint_symbol) {
  compute_main_inner(tint_symbol.local_invocation_index);
  return;
}
