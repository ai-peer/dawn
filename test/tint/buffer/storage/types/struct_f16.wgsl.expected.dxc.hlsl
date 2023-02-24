struct Inner {
  float16_t scalar_f16;
  vector<float16_t, 3> vec3_f16;
  matrix<float16_t, 2, 4> mat2x4_f16;
};
struct S {
  Inner inner;
};

ByteAddressBuffer tint_symbol : register(t0, space0);
RWByteAddressBuffer tint_symbol_1 : register(u1, space0);

matrix<float16_t, 2, 4> tint_symbol_6(uint offset) {
  return matrix<float16_t, 2, 4>(tint_symbol.Load<vector<float16_t, 4> >((offset + 0u)), tint_symbol.Load<vector<float16_t, 4> >((offset + 8u)));
}

Inner tint_symbol_3(uint offset) {
  const Inner tint_symbol_14 = {tint_symbol.Load<float16_t>((offset + 0u)), tint_symbol.Load<vector<float16_t, 3> >((offset + 8u)), tint_symbol_6((offset + 16u))};
  return tint_symbol_14;
}

S tint_symbol_2(uint offset) {
  const S tint_symbol_15 = {tint_symbol_3((offset + 0u))};
  return tint_symbol_15;
}

void tint_symbol_12(uint offset, matrix<float16_t, 2, 4> value) {
  tint_symbol_1.Store<vector<float16_t, 4> >((offset + 0u), value[0u]);
  tint_symbol_1.Store<vector<float16_t, 4> >((offset + 8u), value[1u]);
}

void tint_symbol_9(uint offset, Inner value) {
  tint_symbol_1.Store<float16_t>((offset + 0u), value.scalar_f16);
  tint_symbol_1.Store<vector<float16_t, 3> >((offset + 8u), value.vec3_f16);
  tint_symbol_12((offset + 16u), value.mat2x4_f16);
}

void tint_symbol_8(uint offset, S value) {
  tint_symbol_9((offset + 0u), value.inner);
}

[numthreads(1, 1, 1)]
void main() {
  const S t = tint_symbol_2(0u);
  tint_symbol_8(0u, t);
  return;
}
