identifiers/underscore/prefix/upper/let.wgsl:1:1 warning: use of deprecated language feature: module-scope 'let' has been replaced with 'const'
let A : i32 = 1;
^^^

identifiers/underscore/prefix/upper/let.wgsl:2:1 warning: use of deprecated language feature: module-scope 'let' has been replaced with 'const'
let _A : i32 = 2;
^^^

const A : i32 = 1;

const _A : i32 = 2;

fn f() {
  let B = A;
  let _B = _A;
}
