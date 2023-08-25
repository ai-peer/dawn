[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  {
    int vec3f = 1;
    int b = vec3f;
  }
  float3 c = float3(0.0f, 0.0f, 0.0f);
  float3 d = float3(0.0f, 0.0f, 0.0f);
}
