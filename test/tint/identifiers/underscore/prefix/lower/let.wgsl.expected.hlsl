identifiers/underscore/prefix/lower/let.wgsl:1:1 warning: use of deprecated language feature: module-scope 'let' has been replaced with 'const'
let a : i32 = 1;
^^^

identifiers/underscore/prefix/lower/let.wgsl:2:1 warning: use of deprecated language feature: module-scope 'let' has been replaced with 'const'
let _a : i32 = 2;
^^^

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  const int b = a;
  const int _b = _a;
}
