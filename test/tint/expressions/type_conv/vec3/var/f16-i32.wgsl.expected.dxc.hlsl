[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static vector<float16_t, 3> u = vector<float16_t, 3>(float16_t(1.0h), float16_t(1.0h), float16_t(1.0h));

void f() {
  const int3 v = int3(u);
}
