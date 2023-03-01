[numthreads(1, 1, 1)]
void compute_main() {
  const float a = 1.23000001907348632812f;
  float b = max(a, 1e-38f);
  return;
}
