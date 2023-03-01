#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
float foo() {
  int oob = 99;
  int index = oob;
  bool predicate = (uint(index) < 3u);
  float predicated_load = 0.0f;
  if (predicate) {
    predicated_load = vec4(0.0f)[index];
  }
  float b = predicated_load;
  vec4 v = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  int index_1 = oob;
  bool predicate_1 = (uint(index_1) < 3u);
  if (predicate_1) {
    v[index_1] = b;
  }
  return b;
}

