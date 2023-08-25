[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static float2 arr[2] = {float2(1.0f, 1.0f), float2(2.0f, 2.0f)};

void f() {
  float2 v[2] = arr;
}
