struct str {
  float4 i;
};

void func(inout float4 pointer) {
  pointer = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

[numthreads(1, 1, 1)]
void main() {
  str F = (str)0;
  func(F.i);
  return;
}
