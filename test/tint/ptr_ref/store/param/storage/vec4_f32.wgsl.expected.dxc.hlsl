RWByteAddressBuffer S : register(u0);

void func_S() {
  S.Store4(0u, asuint(float4(0.0f, 0.0f, 0.0f, 0.0f)));
}

[numthreads(1, 1, 1)]
void main() {
  func_S();
  return;
}
