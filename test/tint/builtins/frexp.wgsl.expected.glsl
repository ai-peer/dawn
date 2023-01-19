#version 310 es

struct frexp_result_f32 {
  float fract;
  int exp;
};


void tint_symbol() {
  frexp_result_f32 res = frexp_result_f32(0.61500001f, 1);
  int exp_1 = res.exp;
  float fract_1 = res.fract;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
