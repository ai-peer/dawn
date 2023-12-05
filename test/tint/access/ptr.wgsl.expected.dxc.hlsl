int tint_ftoi(float v) {
  return ((v < 2147483520.0f) ? ((v < -2147483648.0f) ? -2147483648 : int(v)) : 2147483647);
}

RWByteAddressBuffer s : register(u0);
groupshared int g1;

int d(int val) {
  return val;
}

int c(inout int val) {
  const int tint_symbol = val;
  const int tint_symbol_1 = d(val);
  return (tint_symbol + tint_symbol_1);
}

int a(inout int val) {
  const int tint_symbol_2 = val;
  const int tint_symbol_3 = c(val);
  return (tint_symbol_2 + tint_symbol_3);
}

int z() {
  int atomic_result = 0;
  InterlockedOr(g1, 0, atomic_result);
  return atomic_result;
}

int y(inout float3 v1) {
  v1.x = cross(v1, v1).x;
  return tint_ftoi(v1.x);
}

struct S {
  int a;
  int b;
};

int b(inout S val) {
  return (val.a + val.b);
}

struct tint_symbol_11 {
  uint local_invocation_index : SV_GroupIndex;
};

void main_inner(uint local_invocation_index) {
  {
    int atomic_result_1 = 0;
    InterlockedExchange(g1, 0, atomic_result_1);
  }
  GroupMemoryBarrierWithGroupSync();
  int v1 = 0;
  S v2 = (S)0;
  float3 v4 = (0.0f).xxx;
  int atomic_result_2 = 0;
  InterlockedOr(g1, 0, atomic_result_2);
  const int t1 = atomic_result_2;
  const int tint_symbol_4 = a(v1);
  const int tint_symbol_5 = b(v2);
  const int tint_symbol_6 = b(v2);
  const int tint_symbol_7 = z();
  const int tint_symbol_8 = t1;
  const int tint_symbol_9 = y(v4);
  s.Store(0u, asuint((((((tint_symbol_4 + tint_symbol_5) + tint_symbol_6) + tint_symbol_7) + tint_symbol_8) + tint_symbol_9)));
}

[numthreads(1, 1, 1)]
void main(tint_symbol_11 tint_symbol_10) {
  main_inner(tint_symbol_10.local_invocation_index);
  return;
}
