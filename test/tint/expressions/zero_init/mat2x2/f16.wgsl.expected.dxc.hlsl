[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  matrix<float16_t, 2, 2> v = matrix<float16_t, 2, 2>(vector<float16_t, 2>(float16_t(0.0h), float16_t(0.0h)), vector<float16_t, 2>(float16_t(0.0h), float16_t(0.0h)));
}
