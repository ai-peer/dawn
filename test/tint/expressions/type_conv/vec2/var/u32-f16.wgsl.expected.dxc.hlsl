[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static uint2 u = uint2(1u, 1u);

void f() {
  const vector<float16_t, 2> v = vector<float16_t, 2>(u);
}
