statements/discard/atomic_in_for_loop_continuing.wgsl:14:19 warning: 'textureSample' must only be called from uniform control flow
    result += i32(textureSample(t, s, coord).x);
                  ^^^^^^^^^^^^^

statements/discard/atomic_in_for_loop_continuing.wgsl:13:3 note: control flow depends on non-uniform value
  for (var i = 0; i < 10; i = atomicAdd(&a, 1)) {
  ^^^

statements/discard/atomic_in_for_loop_continuing.wgsl:13:42 note: reading from read_write storage buffer 'a' may result in a non-uniform value
  for (var i = 0; i < 10; i = atomicAdd(&a, 1)) {
                                         ^

#version 310 es
precision mediump float;

bool tint_discarded = false;
layout(location = 0) in float tint_symbol_1;
layout(location = 1) in vec2 coord_1;
layout(location = 0) out int value;
layout(binding = 2, std430) buffer a_block_ssbo {
  int inner;
} a;

uniform highp sampler2D t_s;

int foo(float tint_symbol, vec2 coord) {
  if ((tint_symbol == 0.0f)) {
    tint_discarded = true;
  }
  int result = 0;
  {
    int i = 0;
    while (true) {
      if (!((i < 10))) {
        break;
      }
      {
        result = (result + int(texture(t_s, coord).x));
      }
      {
        int tint_symbol_2 = 0;
        if (!(tint_discarded)) {
          tint_symbol_2 = atomicAdd(a.inner, 1);
        }
        i = tint_symbol_2;
      }
    }
  }
  return result;
}

void main() {
  int inner_result = foo(tint_symbol_1, coord_1);
  value = inner_result;
  if (tint_discarded) {
    discard;
  }
  return;
}
