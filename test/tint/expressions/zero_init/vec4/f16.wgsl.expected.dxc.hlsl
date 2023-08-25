[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  vector<float16_t, 4> v = vector<float16_t, 4>(float16_t(0.0h), float16_t(0.0h), float16_t(0.0h), float16_t(0.0h));
}
