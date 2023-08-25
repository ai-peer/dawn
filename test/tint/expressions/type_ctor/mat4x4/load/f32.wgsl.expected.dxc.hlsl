[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  float4x4 m = float4x4(float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 0.0f));
  const float4x4 m_1 = float4x4(m);
}
