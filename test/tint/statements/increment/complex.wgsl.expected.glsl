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
  int tint_symbol_6 = idx1();
  uint tint_symbol_7 = uint(tint_symbol_6);
  int tint_symbol_8 = idx2();
  uint tint_symbol_9 = uint(tint_symbol_8);
  uint tint_symbol_2_save = tint_symbol_7;
  uint tint_symbol_2_save_1 = tint_symbol_9;
  int tint_symbol_10 = idx3();
  uint tint_symbol_3 = uint(tint_symbol_10);
  {
    tint_symbol.inner[tint_symbol_2_save].a[tint_symbol_2_save_1][tint_symbol_3] = (tint_symbol.inner[tint_symbol_2_save].a[tint_symbol_2_save_1][tint_symbol_3] + 1);
    while (true) {
      if (!((v < 10u))) {
        break;
      }
      {
      }
      {
        int tint_symbol_11 = idx4();
        uint tint_symbol_12 = uint(tint_symbol_11);
        int tint_symbol_13 = idx5();
        uint tint_symbol_14 = uint(tint_symbol_13);
        uint tint_symbol_4_save = tint_symbol_12;
        uint tint_symbol_4_save_1 = tint_symbol_14;
        int tint_symbol_15 = idx6();
        uint tint_symbol_5 = uint(tint_symbol_15);
        tint_symbol.inner[tint_symbol_4_save].a[tint_symbol_4_save_1][tint_symbol_5] = (tint_symbol.inner[tint_symbol_4_save].a[tint_symbol_4_save_1][tint_symbol_5] + 1);
      }
    }
  }
}

