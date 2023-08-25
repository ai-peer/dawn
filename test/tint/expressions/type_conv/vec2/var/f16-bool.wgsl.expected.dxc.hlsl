[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static vector<float16_t, 2> u = vector<float16_t, 2>(float16_t(1.0h), float16_t(1.0h));

void f() {
  const bool2 v = bool2(u);
}
