[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

uint3 tint_ftou(float3 v) {
  return ((v < float3(4294967040.0f, 4294967040.0f, 4294967040.0f)) ? ((v < float3(0.0f, 0.0f, 0.0f)) ? uint3(0u, 0u, 0u) : uint3(v)) : uint3(4294967295u, 4294967295u, 4294967295u));
}

static float3 u = float3(1.0f, 1.0f, 1.0f);

void f() {
  const uint3 v = tint_ftou(u);
}
