[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

uint3 tint_ftou(float3 v) {
  return ((v < float3(4294967040.0f, 4294967040.0f, 4294967040.0f)) ? ((v < float3(0.0f, 0.0f, 0.0f)) ? uint3(0u, 0u, 0u) : uint3(v)) : uint3(4294967295u, 4294967295u, 4294967295u));
}

static float t = 0.0f;

float3 m() {
  t = 1.0f;
  return float3((t).xxx);
}

void f() {
  const float3 tint_symbol = m();
  uint3 v = tint_ftou(tint_symbol);
}
