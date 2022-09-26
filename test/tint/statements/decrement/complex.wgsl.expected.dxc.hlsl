[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

RWByteAddressBuffer buffer : register(u0, space0);
static uint v = 0u;

int idx1() {
  v = (v - 1u);
  return 1;
}

int idx2() {
  v = (v - 1u);
  return 2;
}

int idx3() {
  v = (v - 1u);
  return 3;
}

int idx4() {
  v = (v - 1u);
  return 4;
}

int idx5() {
  v = (v - 1u);
  return 0;
}

int idx6() {
  v = (v - 1u);
  return 2;
}

void main() {
  const int tint_symbol_4 = idx1();
  const uint tint_symbol_5 = uint(tint_symbol_4);
  const int tint_symbol_6 = idx2();
  const uint tint_symbol_7 = uint(tint_symbol_6);
  const uint tint_symbol_save = tint_symbol_5;
  const uint tint_symbol_save_1 = tint_symbol_7;
  const int tint_symbol_8 = idx3();
  const uint tint_symbol_1 = uint(tint_symbol_8);
  {
    buffer.Store((((64u * tint_symbol_save) + (16u * tint_symbol_save_1)) + (4u * tint_symbol_1)), asuint((asint(buffer.Load((((64u * tint_symbol_save) + (16u * tint_symbol_save_1)) + (4u * tint_symbol_1)))) - 1)));
    [loop] while (true) {
      if (!((v < 10u))) {
        break;
      }
      {
      }
      {
        const int tint_symbol_9 = idx4();
        const uint tint_symbol_10 = uint(tint_symbol_9);
        const int tint_symbol_11 = idx5();
        const uint tint_symbol_12 = uint(tint_symbol_11);
        const uint tint_symbol_2_save = tint_symbol_10;
        const uint tint_symbol_2_save_1 = tint_symbol_12;
        const int tint_symbol_13 = idx6();
        const uint tint_symbol_3 = uint(tint_symbol_13);
        buffer.Store((((64u * tint_symbol_2_save) + (16u * tint_symbol_2_save_1)) + (4u * tint_symbol_3)), asuint((asint(buffer.Load((((64u * tint_symbol_2_save) + (16u * tint_symbol_2_save_1)) + (4u * tint_symbol_3)))) - 1)));
      }
    }
  }
}
