[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  float3x3 m = float3x3(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f));
  const float3x3 m_1 = float3x3(m);
}
