identifiers/underscore/prefix/upper/let.wgsl:1:1 warning: use of deprecated language feature: module-scope 'let' has been replaced with 'const'
let A : i32 = 1;
^^^

identifiers/underscore/prefix/upper/let.wgsl:2:1 warning: use of deprecated language feature: module-scope 'let' has been replaced with 'const'
let _A : i32 = 2;
^^^

[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

void f() {
  const int B = 1;
  const int _B = 2;
}
