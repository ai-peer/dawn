#version 310 es

layout(binding = 0, std430) buffer Buf_ssbo {
  uint count;
  uint data[50];
} b;

void tint_symbol() {
  uint i = 0u;
  while (true) {
    if ((i >= b.count)) {
      break;
    }
    uint tint_symbol_1 = i;
    uint p_save = tint_symbol_1;
    if (((i % 2u) == 0u)) {
      {
        b.data[tint_symbol_1] = (b.data[tint_symbol_1] * 2u);
        i = (i + 1u);
      }
      continue;
    }
    b.data[tint_symbol_1] = 0u;
    {
      b.data[tint_symbol_1] = (b.data[tint_symbol_1] * 2u);
      i = (i + 1u);
    }
  }
  return;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
