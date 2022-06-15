identifiers/underscore/prefix/lower/let.wgsl:1:1 warning: use of deprecated language feature: module-scope 'let' has been replaced with 'const'
let a : i32 = 1;
^^^

identifiers/underscore/prefix/lower/let.wgsl:2:1 warning: use of deprecated language feature: module-scope 'let' has been replaced with 'const'
let _a : i32 = 2;
^^^

const a : i32 = 1;

const _a : i32 = 2;

fn f() {
  let b = a;
  let _b = _a;
}
