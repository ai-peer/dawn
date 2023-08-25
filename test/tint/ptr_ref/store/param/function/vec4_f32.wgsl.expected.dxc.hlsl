void func(inout float4 pointer) {
  pointer = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

[numthreads(1, 1, 1)]
void main() {
  float4 F = float4(0.0f, 0.0f, 0.0f, 0.0f);
  func(F);
  return;
}
