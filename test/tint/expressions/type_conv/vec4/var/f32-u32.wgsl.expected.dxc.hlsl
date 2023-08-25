[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

uint4 tint_ftou(float4 v) {
  return ((v < float4(4294967040.0f, 4294967040.0f, 4294967040.0f, 4294967040.0f)) ? ((v < float4(0.0f, 0.0f, 0.0f, 0.0f)) ? uint4(0u, 0u, 0u, 0u) : uint4(v)) : uint4(4294967295u, 4294967295u, 4294967295u, 4294967295u));
}

static float4 u = float4(1.0f, 1.0f, 1.0f, 1.0f);

void f() {
  const uint4 v = tint_ftou(u);
}
