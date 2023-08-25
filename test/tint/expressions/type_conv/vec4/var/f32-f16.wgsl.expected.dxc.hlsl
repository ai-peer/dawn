[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static float4 u = float4(1.0f, 1.0f, 1.0f, 1.0f);

void f() {
  const vector<float16_t, 4> v = vector<float16_t, 4>(u);
}
