var<private> a = array<bool, 2>();

fn func() {
  // Should re-use above method
  var a = array<bool, 2>();

  // Not defined above, should get own method
  var z = array<bool, 156>();
}

