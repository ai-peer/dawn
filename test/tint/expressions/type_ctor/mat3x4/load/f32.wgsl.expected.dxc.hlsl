[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  float3x4 m = float3x4(float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f));
  const float3x4 m_1 = float3x4(m);
}
