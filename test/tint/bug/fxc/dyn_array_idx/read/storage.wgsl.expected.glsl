#version 310 es

struct UBO {
  int dynamic_idx;
};

layout(binding = 0) uniform UBO_1 {
  UBO _;
} ubo;

struct Result {
  int tint_symbol;
};

layout(binding = 2, std430) buffer Result_1 {
  Result _;
} result;
struct SSBO {
  int data[4];
};

layout(binding = 1, std430) buffer SSBO_1 {
  SSBO _;
} ssbo;
void f() {
  result._.tint_symbol = ssbo._.data[ubo._.dynamic_idx];
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
