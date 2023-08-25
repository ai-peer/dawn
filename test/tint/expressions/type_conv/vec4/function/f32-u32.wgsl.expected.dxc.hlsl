[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

uint4 tint_ftou(float4 v) {
  return ((v < float4(4294967040.0f, 4294967040.0f, 4294967040.0f, 4294967040.0f)) ? ((v < float4(0.0f, 0.0f, 0.0f, 0.0f)) ? uint4(0u, 0u, 0u, 0u) : uint4(v)) : uint4(4294967295u, 4294967295u, 4294967295u, 4294967295u));
}

static float t = 0.0f;

float4 m() {
  t = 1.0f;
  return float4((t).xxxx);
}

void f() {
  const float4 tint_symbol = m();
  uint4 v = tint_ftou(tint_symbol);
}
