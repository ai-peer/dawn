void func(inout float4 pointer) {
  pointer = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

[numthreads(1, 1, 1)]
void main() {
  float2x4 F = float2x4(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  func(F[1]);
  return;
}
