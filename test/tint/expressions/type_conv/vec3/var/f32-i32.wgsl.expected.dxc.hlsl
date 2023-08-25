[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

int3 tint_ftoi(float3 v) {
  return ((v < float3(2147483520.0f, 2147483520.0f, 2147483520.0f)) ? ((v < float3(-2147483648.0f, -2147483648.0f, -2147483648.0f)) ? int3(-2147483648, -2147483648, -2147483648) : int3(v)) : int3(2147483647, 2147483647, 2147483647));
}

static float3 u = float3(1.0f, 1.0f, 1.0f);

void f() {
  const int3 v = tint_ftoi(u);
}
