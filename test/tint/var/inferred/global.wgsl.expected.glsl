#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
struct MyStruct {
  float f1;
};

int v1 = 1;
uint v2 = 1u;
float v3 = 1.0f;
ivec3 v4 = ivec3(1);
uvec3 v5 = uvec3(1u, 2u, 3u);
vec3 v6 = vec3(1.0f, 2.0f, 3.0f);
MyStruct v7 = MyStruct(1.0f);
float v8[10] = float[10](0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
int v9 = 0;
uint v10 = 0u;
float v11 = 0.0f;
MyStruct v12 = MyStruct(0.0f);
MyStruct v13 = MyStruct(0.0f);
float v14[10] = float[10](0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
ivec3 v15 = ivec3(1, 2, 3);
vec3 v16 = vec3(1.0f, 2.0f, 3.0f);
