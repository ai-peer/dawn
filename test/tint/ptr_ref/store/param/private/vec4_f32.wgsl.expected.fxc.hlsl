void func(inout float4 pointer) {
  pointer = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

static float4 P = float4(0.0f, 0.0f, 0.0f, 0.0f);

[numthreads(1, 1, 1)]
void main() {
  func(P);
  return;
}
