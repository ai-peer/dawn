[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static int3 u = int3(1, 1, 1);

void f() {
  const vector<float16_t, 3> v = vector<float16_t, 3>(u);
}
