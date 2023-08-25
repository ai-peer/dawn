[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static uint4 u = uint4(1u, 1u, 1u, 1u);

void f() {
  const vector<float16_t, 4> v = vector<float16_t, 4>(u);
}
