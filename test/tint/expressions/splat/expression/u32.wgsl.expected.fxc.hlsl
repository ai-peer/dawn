[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  uint2 v2 = uint2(3u, 3u);
  uint3 v3 = uint3(3u, 3u, 3u);
  uint4 v4 = uint4(3u, 3u, 3u, 3u);
}
