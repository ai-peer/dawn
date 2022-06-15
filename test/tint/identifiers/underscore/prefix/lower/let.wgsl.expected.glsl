identifiers/underscore/prefix/lower/let.wgsl:1:1 warning: use of deprecated language feature: module-scope 'let' has been replaced with 'const'
let a : i32 = 1;
^^^

identifiers/underscore/prefix/lower/let.wgsl:2:1 warning: use of deprecated language feature: module-scope 'let' has been replaced with 'const'
let _a : i32 = 2;
^^^

#version 310 es

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void unused_entry_point() {
  return;
}
void f() {
  int b = a;
  int _b = _a;
}

