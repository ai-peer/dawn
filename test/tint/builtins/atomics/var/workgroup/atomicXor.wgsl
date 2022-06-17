var<workgroup> wg: atomic<u32>;

// @compute @workgroup_size(1)
// fn compute_main() {
//   let r = atomicXor(&wg, 1u);

//   compute_main_2();
// }


var<workgroup> wg_u32: atomic<u32>;
var<workgroup> wg_i32: atomic<i32>;
var<workgroup> wg_f32: atomic<i32>;

fn compute_main_2() {
  // TODO: move to separate files
  // {let r = atomicLoad(&wg_i32);}
  // {let r = atomicLoad(&wg_u32);}
  
  // {atomicStore(&wg_i32, 99i);}
  // {atomicStore(&wg_u32, 99u);}

  // {let r = atomicExchange(&wg_i32, 123i);}
  // {let r = atomicExchange(&wg_u32, 123u);}

  {atomicCompareExchangeWeak(&wg_u32, 123u, 456u);}
  // {let r = atomicCompareExchangeWeak(&wg_i32, 123i, 456i);}

  // {let r = atomicAdd(&wg, 1u);}
  // {let r = atomicSub(&wg, 1u);}
  // {let r = atomicMin(&wg, 1u);}
  // {let r = atomicMax(&wg, 1u);}
  // {let r = atomicAnd(&wg, 1u);}
  // {let r = atomicOr(&wg, 1u);}
  // {let r = atomicXor(&wg, 1u);}

  // {let r = atomicAdd(&wg_i32, 1i);}
  // {let r = atomicSub(&wg_i32, 1i);}
  // {let r = atomicMin(&wg_i32, 1i);}
  // {let r = atomicMax(&wg_i32, 1i);}
  // {let r = atomicAnd(&wg_i32, 1i);}
  // {let r = atomicOr(&wg_i32, 1i);}
  // {let r = atomicXor(&wg_i32, 2i);}
}