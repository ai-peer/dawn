[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  float3x2 m = float3x2(float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f));
  const float3x2 m_1 = float3x2(m);
}
