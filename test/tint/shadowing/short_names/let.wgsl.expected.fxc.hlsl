[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  {
    const int vec3f = 1;
    const int b = vec3f;
  }
  const float3 c = float3(0.0f, 0.0f, 0.0f);
  const float3 d = float3(0.0f, 0.0f, 0.0f);
}
