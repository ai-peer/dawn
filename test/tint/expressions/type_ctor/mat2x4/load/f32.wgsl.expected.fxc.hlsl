[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  float2x4 m = float2x4(float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f));
  const float2x4 m_1 = float2x4(m);
}
