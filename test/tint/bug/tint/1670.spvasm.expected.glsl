#version 310 es
precision highp float;
precision highp int;

void main_1() {
  mat2 m2i = mat2(0.0f, 0.0f, 0.0f, 0.0f);
  mat2 m2 = mat2(0.0f, 0.0f, 0.0f, 0.0f);
  mat3 m3i = mat3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  mat3 m3 = mat3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  mat4 m4i = mat4(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  mat4 m4 = mat4(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  float s = (1.0f / determinant(m2));
  m2i = mat2(vec2((s * m2[1u][1u]), (-(s) * m2[0u][1u])), vec2((-(s) * m2[1u][0u]), (s * m2[0u][0u])));
  m3i = ((1.0f / determinant(m3)) * mat3(vec3(((m3[1u][1u] * m3[2u][2u]) - (m3[1u][2u] * m3[2u][1u])), ((m3[0u][2u] * m3[2u][1u]) - (m3[0u][1u] * m3[2u][2u])), ((m3[0u][1u] * m3[1u][2u]) - (m3[0u][2u] * m3[1u][1u]))), vec3(((m3[1u][2u] * m3[2u][0u]) - (m3[1u][0u] * m3[2u][2u])), ((m3[0u][0u] * m3[2u][2u]) - (m3[0u][2u] * m3[2u][0u])), ((m3[0u][2u] * m3[1u][0u]) - (m3[0u][0u] * m3[1u][2u]))), vec3(((m3[1u][0u] * m3[2u][1u]) - (m3[1u][1u] * m3[2u][0u])), ((m3[0u][1u] * m3[2u][0u]) - (m3[0u][0u] * m3[2u][1u])), ((m3[0u][0u] * m3[1u][1u]) - (m3[0u][1u] * m3[1u][0u])))));
  m4i = ((1.0f / determinant(m4)) * mat4(vec4((((m4[1u][1u] * ((m4[2u][2u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][2u]))) - (m4[1u][2u] * ((m4[2u][1u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][1u])))) + (m4[1u][3u] * ((m4[2u][1u] * m4[3u][2u]) - (m4[2u][2u] * m4[3u][1u])))), (((-(m4[0u][1u]) * ((m4[2u][2u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][2u]))) + (m4[0u][2u] * ((m4[2u][1u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][1u])))) - (m4[0u][3u] * ((m4[2u][1u] * m4[3u][2u]) - (m4[2u][2u] * m4[3u][1u])))), (((m4[0u][1u] * ((m4[1u][2u] * m4[3u][3u]) - (m4[1u][3u] * m4[3u][2u]))) - (m4[0u][2u] * ((m4[1u][1u] * m4[3u][3u]) - (m4[1u][3u] * m4[3u][1u])))) + (m4[0u][3u] * ((m4[1u][1u] * m4[3u][2u]) - (m4[1u][2u] * m4[3u][1u])))), (((-(m4[0u][1u]) * ((m4[1u][2u] * m4[2u][3u]) - (m4[1u][3u] * m4[2u][2u]))) + (m4[0u][2u] * ((m4[1u][1u] * m4[2u][3u]) - (m4[1u][3u] * m4[2u][1u])))) - (m4[0u][3u] * ((m4[1u][1u] * m4[2u][2u]) - (m4[1u][2u] * m4[2u][1u]))))), vec4((((-(m4[1u][0u]) * ((m4[2u][2u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][2u]))) + (m4[1u][2u] * ((m4[2u][0u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][0u])))) - (m4[1u][3u] * ((m4[2u][0u] * m4[3u][2u]) - (m4[2u][2u] * m4[3u][0u])))), (((m4[0u][0u] * ((m4[2u][2u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][2u]))) - (m4[0u][2u] * ((m4[2u][0u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][0u])))) + (m4[0u][3u] * ((m4[2u][0u] * m4[3u][2u]) - (m4[2u][2u] * m4[3u][0u])))), (((-(m4[0u][0u]) * ((m4[1u][2u] * m4[3u][3u]) - (m4[1u][3u] * m4[3u][2u]))) + (m4[0u][2u] * ((m4[1u][0u] * m4[3u][3u]) - (m4[1u][3u] * m4[3u][0u])))) - (m4[0u][3u] * ((m4[1u][0u] * m4[3u][2u]) - (m4[1u][2u] * m4[3u][0u])))), (((m4[0u][0u] * ((m4[1u][2u] * m4[2u][3u]) - (m4[1u][3u] * m4[2u][2u]))) - (m4[0u][2u] * ((m4[1u][0u] * m4[2u][3u]) - (m4[1u][3u] * m4[2u][0u])))) + (m4[0u][3u] * ((m4[1u][0u] * m4[2u][2u]) - (m4[1u][2u] * m4[2u][0u]))))), vec4((((m4[1u][0u] * ((m4[2u][1u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][1u]))) - (m4[1u][1u] * ((m4[2u][0u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][0u])))) + (m4[1u][3u] * ((m4[2u][0u] * m4[3u][1u]) - (m4[2u][1u] * m4[3u][0u])))), (((-(m4[0u][0u]) * ((m4[2u][1u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][1u]))) + (m4[0u][1u] * ((m4[2u][0u] * m4[3u][3u]) - (m4[2u][3u] * m4[3u][0u])))) - (m4[0u][3u] * ((m4[2u][0u] * m4[3u][1u]) - (m4[2u][1u] * m4[3u][0u])))), (((m4[0u][0u] * ((m4[1u][1u] * m4[3u][3u]) - (m4[1u][3u] * m4[3u][1u]))) - (m4[0u][1u] * ((m4[1u][0u] * m4[3u][3u]) - (m4[1u][3u] * m4[3u][0u])))) + (m4[0u][3u] * ((m4[1u][0u] * m4[3u][1u]) - (m4[1u][1u] * m4[3u][0u])))), (((-(m4[0u][0u]) * ((m4[1u][1u] * m4[2u][3u]) - (m4[1u][3u] * m4[2u][1u]))) + (m4[0u][1u] * ((m4[1u][0u] * m4[2u][3u]) - (m4[1u][3u] * m4[2u][0u])))) - (m4[0u][3u] * ((m4[1u][0u] * m4[2u][1u]) - (m4[1u][1u] * m4[2u][0u]))))), vec4((((-(m4[1u][0u]) * ((m4[2u][1u] * m4[3u][2u]) - (m4[2u][2u] * m4[3u][1u]))) + (m4[1u][1u] * ((m4[2u][0u] * m4[3u][2u]) - (m4[2u][2u] * m4[3u][0u])))) - (m4[1u][2u] * ((m4[2u][0u] * m4[3u][1u]) - (m4[2u][1u] * m4[3u][0u])))), (((m4[0u][0u] * ((m4[2u][1u] * m4[3u][2u]) - (m4[2u][2u] * m4[3u][1u]))) - (m4[0u][1u] * ((m4[2u][0u] * m4[3u][2u]) - (m4[2u][2u] * m4[3u][0u])))) + (m4[0u][2u] * ((m4[2u][0u] * m4[3u][1u]) - (m4[2u][1u] * m4[3u][0u])))), (((-(m4[0u][0u]) * ((m4[1u][1u] * m4[3u][2u]) - (m4[1u][2u] * m4[3u][1u]))) + (m4[0u][1u] * ((m4[1u][0u] * m4[3u][2u]) - (m4[1u][2u] * m4[3u][0u])))) - (m4[0u][2u] * ((m4[1u][0u] * m4[3u][1u]) - (m4[1u][1u] * m4[3u][0u])))), (((m4[0u][0u] * ((m4[1u][1u] * m4[2u][2u]) - (m4[1u][2u] * m4[2u][1u]))) - (m4[0u][1u] * ((m4[1u][0u] * m4[2u][2u]) - (m4[1u][2u] * m4[2u][0u])))) + (m4[0u][2u] * ((m4[1u][0u] * m4[2u][1u]) - (m4[1u][1u] * m4[2u][0u])))))));
  return;
}

void tint_symbol() {
  main_1();
}

void main() {
  tint_symbol();
  return;
}