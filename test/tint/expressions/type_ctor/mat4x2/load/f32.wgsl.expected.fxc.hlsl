[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  float4x2 m = float4x2(float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f), float2(0.0f, 0.0f));
  const float4x2 m_1 = float4x2(m);
}
