void set_float4(inout float4 vec, int idx, float val) {
  vec = (idx.xxxx == int4(0, 1, 2, 3)) ? val.xxxx : vec;
}

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

float foo() {
  const int oob = 99;
  const int index = oob;
  const bool predicate = (uint(index) <= 3u);
  float predicated_expr = 0.0f;
  if (predicate) {
    predicated_expr = (0.0f).xxxx[index];
  }
  const float b = predicated_expr;
  float4 v = float4(0.0f, 0.0f, 0.0f, 0.0f);
  const int index_1 = oob;
  const bool predicate_1 = (uint(index_1) <= 3u);
  if (predicate_1) {
    set_float4(v, index_1, b);
  }
  return b;
}
