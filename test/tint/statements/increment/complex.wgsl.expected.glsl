#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
struct S {
  ivec4 a[4];
};

layout(binding = 0, std430) buffer tint_symbol_block_ssbo {
  S inner[];
} tint_symbol;

uint v = 0u;
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

void tint_symbol_1() {
  int tint_symbol_2 = idx1();
  int tint_symbol_3 = idx2();
  int tint_symbol_4 = idx3();
  {
    uint tint_symbol_8_save = uint(tint_symbol_2);
    uint tint_symbol_8_save_1 = uint(tint_symbol_3);
    uint tint_symbol_9 = uint(tint_symbol_4);
    tint_symbol.inner[tint_symbol_8_save].a[tint_symbol_8_save_1][tint_symbol_9] = (tint_symbol.inner[tint_symbol_8_save].a[tint_symbol_8_save_1][tint_symbol_9] + 1);
    while (true) {
      if (!((v < 10u))) {
        break;
      }
      {
      }
      {
        int tint_symbol_5 = idx4();
        int tint_symbol_6 = idx5();
        int tint_symbol_7 = idx6();
        uint tint_symbol_10_save = uint(tint_symbol_5);
        uint tint_symbol_10_save_1 = uint(tint_symbol_6);
        uint tint_symbol_11 = uint(tint_symbol_7);
        tint_symbol.inner[tint_symbol_10_save].a[tint_symbol_10_save_1][tint_symbol_11] = (tint_symbol.inner[tint_symbol_10_save].a[tint_symbol_10_save_1][tint_symbol_11] + 1);
      }
    }
  }
}

