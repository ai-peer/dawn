[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static uint3 u = uint3(1u, 1u, 1u);

void f() {
  const vector<float16_t, 3> v = vector<float16_t, 3>(u);
}
