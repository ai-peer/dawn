struct strided_arr {
  float el;
};
struct strided_arr_1 {
  strided_arr el[3][2];
};

RWByteAddressBuffer s : register(u0, space0);

strided_arr tint_symbol_4(uint offset) {
  const strided_arr tint_symbol_12 = {asfloat(s.Load((offset + 0u)))};
  return tint_symbol_12;
}

typedef strided_arr tint_symbol_3_ret[2];
tint_symbol_3_ret tint_symbol_3(uint offset) {
  strided_arr arr[2] = (strided_arr[2])0;
  {
    for(uint i = 0u; (i < 2u); i = (i + 1u)) {
      arr[i] = tint_symbol_4((offset + (i * 8u)));
    }
  }
  return arr;
}

typedef strided_arr tint_symbol_2_ret[3][2];
tint_symbol_2_ret tint_symbol_2(uint offset) {
  strided_arr arr_1[3][2] = (strided_arr[3][2])0;
  {
    for(uint i_1 = 0u; (i_1 < 3u); i_1 = (i_1 + 1u)) {
      arr_1[i_1] = tint_symbol_3((offset + (i_1 * 16u)));
    }
  }
  return arr_1;
}

strided_arr_1 tint_symbol_1(uint offset) {
  const strided_arr_1 tint_symbol_13 = {tint_symbol_2((offset + 0u))};
  return tint_symbol_13;
}

typedef strided_arr_1 tint_symbol_ret[4];
tint_symbol_ret tint_symbol(uint offset) {
  strided_arr_1 arr_2[4] = (strided_arr_1[4])0;
  {
    for(uint i_2 = 0u; (i_2 < 4u); i_2 = (i_2 + 1u)) {
      arr_2[i_2] = tint_symbol_1((offset + (i_2 * 128u)));
    }
  }
  return arr_2;
}

void tint_symbol_10(uint offset, strided_arr value) {
  s.Store((offset + 0u), asuint(value.el));
}

void tint_symbol_9(uint offset, strided_arr value[2]) {
  strided_arr array_3[2] = value;
  {
    for(uint i_3 = 0u; (i_3 < 2u); i_3 = (i_3 + 1u)) {
      tint_symbol_10((offset + (i_3 * 8u)), array_3[i_3]);
    }
  }
}

void tint_symbol_8(uint offset, strided_arr value[3][2]) {
  strided_arr array_2[3][2] = value;
  {
    for(uint i_4 = 0u; (i_4 < 3u); i_4 = (i_4 + 1u)) {
      tint_symbol_9((offset + (i_4 * 16u)), array_2[i_4]);
    }
  }
}

void tint_symbol_7(uint offset, strided_arr_1 value) {
  tint_symbol_8((offset + 0u), value.el);
}

void tint_symbol_6(uint offset, strided_arr_1 value[4]) {
  strided_arr_1 array_1[4] = value;
  {
    for(uint i_5 = 0u; (i_5 < 4u); i_5 = (i_5 + 1u)) {
      tint_symbol_7((offset + (i_5 * 128u)), array_1[i_5]);
    }
  }
}

void f_1() {
  const strided_arr_1 x_19[4] = tint_symbol(0u);
  const strided_arr x_24[3][2] = tint_symbol_2(384u);
  const strided_arr x_28[2] = tint_symbol_3(416u);
  const float x_32 = asfloat(s.Load(424u));
  const strided_arr_1 tint_symbol_14[4] = (strided_arr_1[4])0;
  tint_symbol_6(0u, tint_symbol_14);
  s.Store(424u, asuint(5.0f));
  return;
}

[numthreads(1, 1, 1)]
void f() {
  f_1();
  return;
}
