struct atomic_compare_exchange_result_i32 {
  int old_value;
  bool exchanged;
};
static bool tint_discarded = false;
RWByteAddressBuffer a : register(u0);

struct tint_symbol {
  int value : SV_Target0;
};

atomic_compare_exchange_result_i32 aatomicCompareExchangeWeak(uint offset, int compare, int value) {
  atomic_compare_exchange_result_i32 result=(atomic_compare_exchange_result_i32)0;
  a.InterlockedCompareExchange(offset, compare, value, result.old_value);
  result.exchanged = result.old_value == compare;
  return result;
}


int foo_inner() {
  tint_discarded = true;
  int x = 0;
  atomic_compare_exchange_result_i32 tint_symbol_1 = (atomic_compare_exchange_result_i32)0;
  if (!(tint_discarded)) {
<<<<<<< HEAD   (4874ce [tint][msl] Fix C++17 warning.)
    const atomic_compare_exchange_result_i32 tint_symbol_3 = aatomicCompareExchangeWeak(0u, 0, 1);
    tint_symbol_1.old_value = tint_symbol_3.old_value;
    tint_symbol_1.exchanged = tint_symbol_3.exchanged;
=======
    tint_symbol_1 = aatomicCompareExchangeWeak(0u, 0, 1);
>>>>>>> CHANGE (f83de0 [tint][ast] Fix DemoteToHelper with atomicCompareExchangeWea)
  }
<<<<<<< HEAD   (4874ce [tint][msl] Fix C++17 warning.)
  const tint_symbol_2 result = tint_symbol_1;
=======
  atomic_compare_exchange_result_i32 result = tint_symbol_1;
>>>>>>> CHANGE (f83de0 [tint][ast] Fix DemoteToHelper with atomicCompareExchangeWea)
  if (result.exchanged) {
    x = result.old_value;
  }
  return x;
}

tint_symbol foo() {
  const int inner_result = foo_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  if (tint_discarded) {
    discard;
  }
  return wrapper_result;
}
