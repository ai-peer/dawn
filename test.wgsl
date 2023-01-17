fn f() {
    var x0 = a( b<c,d>(e) );
    var x1 = a< b>c >(d);
    var x2 = a< b<c, d<e> >(f);
    var x3 = a< b<c>() >();
}

// test.wgsl:2:18 error: TEMPLATE ARGS
//     var x0 = a( b<c,d>(e) );
//                  ^^^^^
//
// test.wgsl:3:15 error: TEMPLATE ARGS
//     var x1 = a< b>c >(d);
//               ^^^^^^^
//
// test.wgsl:4:23 error: TEMPLATE ARGS
//     var x2 = a< b<c, d<e> >(f);
//                       ^^^
//
// test.wgsl:4:18 error: TEMPLATE ARGS
//     var x2 = a< b<c, d<e> >(f);
//                  ^^^^^^^^^^
//
// test.wgsl:4:23 error: TEMPLATE ARGS
//     var x2 = a< b<c, d<e> >(f);
//                       ^^^
//
// test.wgsl:4:18 error: TEMPLATE ARGS
//     var x2 = a< b<c, d<e> >(f);
//                  ^^^^^^^^^^
//
// test.wgsl:5:18 error: TEMPLATE ARGS
//     var x3 = a< b<c>() >();
//                  ^^^
//
// test.wgsl:5:15 error: TEMPLATE ARGS
//     var x3 = a< b<c>() >();
//               ^^^^^^^^^^
