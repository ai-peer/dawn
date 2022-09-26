[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

RWByteAddressBuffer buffer : register(u0, space0);
static uint v = 0u;

int idx1() {
  v = (v + 1u);
  return 1;
}

int idx2() {
  v = (v + 1u);
  return 2;
}

int idx3() {
  v = (v + 1u);
  return 3;
}

int idx4() {
  v = (v + 1u);
  return 4;
}

int idx5() {
  v = (v + 1u);
  return 0;
}

int idx6() {
  v = (v + 1u);
  return 2;
}

void main() {
  const int tint_symbol = idx1();
  const int tint_symbol_1 = idx2();
  const int tint_symbol_2 = idx3();
  {
    const uint tint_symbol_6_save = uint(tint_symbol);
    const uint tint_symbol_6_save_1 = uint(tint_symbol_1);
    const uint tint_symbol_7 = uint(tint_symbol_2);
    buffer.Store((((64u * tint_symbol_6_save) + (16u * tint_symbol_6_save_1)) + (4u * tint_symbol_7)), asuint((asint(buffer.Load((((64u * tint_symbol_6_save) + (16u * tint_symbol_6_save_1)) + (4u * tint_symbol_7)))) + 1)));
    [loop] while (true) {
      if (!((v < 10u))) {
        break;
      }
      {
      }
      {
        const int tint_symbol_3 = idx4();
        const int tint_symbol_4 = idx5();
        const int tint_symbol_5 = idx6();
        const uint tint_symbol_8_save = uint(tint_symbol_3);
        const uint tint_symbol_8_save_1 = uint(tint_symbol_4);
        const uint tint_symbol_9 = uint(tint_symbol_5);
        buffer.Store((((64u * tint_symbol_8_save) + (16u * tint_symbol_8_save_1)) + (4u * tint_symbol_9)), asuint((asint(buffer.Load((((64u * tint_symbol_8_save) + (16u * tint_symbol_8_save_1)) + (4u * tint_symbol_9)))) + 1)));
      }
    }
  }
}
