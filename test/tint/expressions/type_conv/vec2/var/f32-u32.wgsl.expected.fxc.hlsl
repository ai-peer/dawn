[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

uint2 tint_ftou(float2 v) {
  return ((v < float2(4294967040.0f, 4294967040.0f)) ? ((v < float2(0.0f, 0.0f)) ? uint2(0u, 0u) : uint2(v)) : uint2(4294967295u, 4294967295u));
}

static float2 u = float2(1.0f, 1.0f);

void f() {
  const uint2 v = tint_ftou(u);
}
