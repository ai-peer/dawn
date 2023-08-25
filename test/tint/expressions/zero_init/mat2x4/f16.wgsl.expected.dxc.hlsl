[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  matrix<float16_t, 2, 4> v = matrix<float16_t, 2, 4>(vector<float16_t, 4>(float16_t(0.0h), float16_t(0.0h), float16_t(0.0h), float16_t(0.0h)), vector<float16_t, 4>(float16_t(0.0h), float16_t(0.0h), float16_t(0.0h), float16_t(0.0h)));
}
