struct str {
  float4 i;
};

void func(inout float4 pointer) {
  pointer = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

static str P = (str)0;

[numthreads(1, 1, 1)]
void main() {
  func(P.i);
  return;
}
