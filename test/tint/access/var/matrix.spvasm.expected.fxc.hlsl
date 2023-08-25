void main_1() {
  float3x3 m = float3x3(float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f));
  const float x_16 = m[1].y;
  return;
}

[numthreads(1, 1, 1)]
void main() {
  main_1();
  return;
}
