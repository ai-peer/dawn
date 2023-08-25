[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  matrix<float16_t, 2, 3> m = matrix<float16_t, 2, 3>(vector<float16_t, 3>(float16_t(0.0h), float16_t(0.0h), float16_t(0.0h)), vector<float16_t, 3>(float16_t(0.0h), float16_t(0.0h), float16_t(0.0h)));
  const matrix<float16_t, 2, 3> m_1 = matrix<float16_t, 2, 3>(m);
}
