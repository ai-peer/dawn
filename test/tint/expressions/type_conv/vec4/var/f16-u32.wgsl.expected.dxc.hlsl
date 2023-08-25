[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static vector<float16_t, 4> u = vector<float16_t, 4>(float16_t(1.0h), float16_t(1.0h), float16_t(1.0h), float16_t(1.0h));

void f() {
  const uint4 v = uint4(u);
}
