package tree_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/cts/result/tree"
	"github.com/go-test/deep"
)

func TestNewSingle(t *testing.T) {
	type Test struct {
		in     result.Result
		expect *tree.Tree
	}
	for _, test := range []Test{
		{
			in: result.Result{
				Name: `suite:*`, Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: map[string]*tree.Node{
						`suite`: {
							Name:  `suite`,
							Level: tree.Suite,
							Results: map[result.Variant]result.Status{
								{OS: "A", GPU: "X"}: result.Pass,
							},
						},
					},
				},
			},
		},
		{
			in: result.Result{
				Name: `suite:a,*`, Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: map[string]*tree.Node{
						`suite`: {
							Name:  `suite`,
							Level: tree.Suite,
							Children: map[string]*tree.Node{
								`a`: {
									Name:  `suite:a`,
									Level: tree.File,
									Results: map[result.Variant]result.Status{
										{OS: "A", GPU: "X"}: result.Pass,
									},
								},
							},
						},
					},
				},
			},
		},
		{
			in: result.Result{
				Name: `suite:a,b:*`, Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: map[string]*tree.Node{
						`suite`: {
							Name:  `suite`,
							Level: tree.Suite,
							Children: map[string]*tree.Node{
								`a`: {
									Name:  `suite:a`,
									Level: tree.File,
									Children: map[string]*tree.Node{
										`b`: {
											Name:  `suite:a,b`,
											Level: tree.File,
											Results: map[result.Variant]result.Status{
												{OS: "A", GPU: "X"}: result.Pass,
											},
										},
									},
								},
							},
						},
					},
				},
			},
		},
		{
			in: result.Result{
				Name: `suite:a,b:c:*`, Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: map[string]*tree.Node{
						`suite`: {
							Name:  `suite`,
							Level: tree.Suite,
							Children: map[string]*tree.Node{
								`a`: {
									Name:  `suite:a`,
									Level: tree.File,
									Children: map[string]*tree.Node{
										`b`: {
											Name:  `suite:a,b`,
											Level: tree.File,
											Children: map[string]*tree.Node{
												`c`: {
													Name:  `suite:a,b:c`,
													Level: tree.Test,
													Results: map[result.Variant]result.Status{
														{OS: "A", GPU: "X"}: result.Pass,
													},
												},
											},
										},
									},
								},
							},
						},
					},
				},
			},
		},
		{
			in: result.Result{
				Name: `suite:a,b,c:d,e:f="g";h=[1,2,3];i=4;*`, Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: map[string]*tree.Node{
						`suite`: {
							Name:  `suite`,
							Level: tree.Suite,
							Children: map[string]*tree.Node{
								`a`: {
									Name:  `suite:a`,
									Level: tree.File,
									Children: map[string]*tree.Node{
										`b`: {
											Name:  `suite:a,b`,
											Level: tree.File,
											Children: map[string]*tree.Node{
												`c`: {
													Name:  `suite:a,b,c`,
													Level: tree.File,
													Children: map[string]*tree.Node{
														`d`: {
															Name:  `suite:a,b,c:d`,
															Level: tree.Test,
															Children: map[string]*tree.Node{
																`e`: {
																	Name:  `suite:a,b,c:d,e`,
																	Level: tree.Test,
																	Children: map[string]*tree.Node{
																		`f="g";h=[1,2,3];i=4`: {
																			Name:  `suite:a,b,c:d,e:f="g";h=[1,2,3];i=4`,
																			Level: tree.Case,
																			Results: map[result.Variant]result.Status{
																				{OS: "A", GPU: "X"}: result.Pass,
																			},
																		},
																	},
																},
															},
														},
													},
												},
											},
										},
									},
								},
							},
						},
					},
				},
			},
		},
		{
			in: result.Result{
				Name: `suite:a,b:c:d="e";*`, Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: map[string]*tree.Node{
						`suite`: {
							Name:  `suite`,
							Level: tree.Suite,
							Children: map[string]*tree.Node{
								`a`: {
									Name:  `suite:a`,
									Level: tree.File,
									Children: map[string]*tree.Node{
										`b`: {
											Name:  `suite:a,b`,
											Level: tree.File,
											Children: map[string]*tree.Node{
												`c`: {
													Name:  `suite:a,b:c`,
													Level: tree.Test,
													Children: map[string]*tree.Node{
														`d="e"`: {
															Name:  `suite:a,b:c:d="e"`,
															Level: tree.Case,
															Results: map[result.Variant]result.Status{
																{OS: "A", GPU: "X"}: result.Pass,
															},
														},
													},
												},
											},
										},
									},
								},
							},
						},
					},
				},
			},
		},
	} {

		got, err := tree.New([]result.Result{test.in})
		if err != nil {
			t.Fatalf("New(%v) returned %v", test.in, err)
		}
		if diff := deep.Equal(got, test.expect); diff != nil {
			t.Errorf("New(%v) tree was not as expected:\n%v", test.in, diff)
		}
	}

}

func TestNewMultiple(t *testing.T) {
	got, err := tree.New([]result.Result{
		{Name: `suite:a,b:c:d="e";*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:h,b:c:f="g";*`, Status: result.Abort, Variant: result.Variant{OS: "A", GPU: "X"}},
		{Name: `suite:a,b:c:f="g";*`, Status: result.Skip, Variant: result.Variant{OS: "A", GPU: "Y"}},
		{Name: `suite:a,b:c:d="e";*`, Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"}},
	})
	if err != nil {
		t.Errorf("New() returned %v", err)
	}
	expect := &tree.Tree{
		Node: tree.Node{
			Children: map[string]*tree.Node{
				`suite`: {
					Name:  `suite`,
					Level: tree.Suite,
					Children: map[string]*tree.Node{
						`a`: {
							Name:  `suite:a`,
							Level: tree.File,
							Children: map[string]*tree.Node{
								`b`: {
									Name:  `suite:a,b`,
									Level: tree.File,
									Children: map[string]*tree.Node{
										`c`: {
											Name:  `suite:a,b:c`,
											Level: tree.Test,
											Children: map[string]*tree.Node{
												`d="e"`: {
													Name:  `suite:a,b:c:d="e"`,
													Level: tree.Case,
													Results: map[result.Variant]result.Status{
														{OS: "A", GPU: "X"}: result.Pass,
														{OS: "B", GPU: "X"}: result.Fail,
													},
												},
												`f="g"`: {
													Name:  `suite:a,b:c:f="g"`,
													Level: tree.Case,
													Results: map[result.Variant]result.Status{
														{OS: "A", GPU: "Y"}: result.Skip,
													},
												},
											},
										},
									},
								},
							},
						},
						`h`: {
							Name:  `suite:h`,
							Level: tree.File,
							Children: map[string]*tree.Node{
								`b`: {
									Name:  `suite:h,b`,
									Level: tree.File,
									Children: map[string]*tree.Node{
										`c`: {
											Name:  `suite:h,b:c`,
											Level: tree.Test,
											Children: map[string]*tree.Node{
												`f="g"`: {
													Name:  `suite:h,b:c:f="g"`,
													Level: tree.Case,
													Results: map[result.Variant]result.Status{
														{OS: "A", GPU: "X"}: result.Abort,
													},
												},
											},
										},
									},
								},
							},
						},
					},
				},
			},
		},
	}
	if diff := deep.Equal(got, expect); diff != nil {
		t.Errorf("New() tree was not as expected:\n%v", diff)
		t.Errorf("got:\n%v", got)
		t.Errorf("expect:\n%v", expect)
	}
}

func TestList(t *testing.T) {
	tree, err := tree.New([]result.Result{
		{Name: `suite:*`, Status: result.Skip, Variant: result.Variant{OS: "A", GPU: "Y"}},
		{Name: `suite:a,*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:a,b,*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:a,b:c:*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:a,b:c:d;*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:a,b:c:d="e";*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:h,b:c:f="g";*`, Status: result.Abort, Variant: result.Variant{OS: "A", GPU: "X"}},
		{Name: `suite:a,b:c:f="g";*`, Status: result.Skip, Variant: result.Variant{OS: "A", GPU: "Y"}},
		{Name: `suite:a,b:c:d="e";*`, Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"}},
	})
	if err != nil {
		t.Fatalf("New() returned %v", err)
	}
	got := tree.List()
	expect := []result.Result{
		{Name: `suite:*`, Status: result.Skip, Variant: result.Variant{OS: "A", GPU: "Y"}},
		{Name: `suite:a,*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:a,b,*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:a,b:c:*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:a,b:c:d;*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:a,b:c:d="e";*`, Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"}},
		{Name: `suite:a,b:c:d="e";*`, Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Name: `suite:a,b:c:f="g";*`, Status: result.Skip, Variant: result.Variant{OS: "A", GPU: "Y"}},
		{Name: `suite:h,b:c:f="g";*`, Status: result.Abort, Variant: result.Variant{OS: "A", GPU: "X"}},
	}
	if diff := deep.Equal(got, expect); diff != nil {
		t.Errorf("List() was not as expected:\n%v", diff)
	}
}

func TestReduce(t *testing.T) {
	type Test struct {
		name   string
		in     []result.Result
		expect []result.Result
	}
	for _, test := range []Test{
		{
			name: "Sibling tests, different variants",
			in: []result.Result{
				{Name: `suite:a,b,*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,c,*`, Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
			},
			expect: []result.Result{
				{Name: `suite:a,b,*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,c,*`, Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
			},
		},
		{
			name: "Sibling tests, same variants",
			in: []result.Result{
				{Name: `suite:a,b,*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,c,*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,c,*`, Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
			},
			expect: []result.Result{
				{Name: `suite:*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,c,*`, Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
			},
		},
		{
			name: "Sibling tests, same variants",
			in: []result.Result{
				{Name: `suite:a,b,x,*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,b,y,*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,c,*`, Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
			expect: []result.Result{
				{Name: `suite:a,b,*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,c,*`, Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
		},
		{
			name: "Sibling cases, different variants",
			in: []result.Result{
				{Name: `suite:a,b:x:*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,b:y:*`, Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
			},
			expect: []result.Result{
				{Name: `suite:a,b:x:*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,b:y:*`, Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
			},
		},
		{
			name: "Sibling cases, same variants",
			in: []result.Result{
				{Name: `suite:a,b:x:*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,b:y:*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,b:y:*`, Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
			},
			expect: []result.Result{
				{Name: `suite:*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,b:y:*`, Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
			},
		},
		{
			name: "test + cases, same variants",
			in: []result.Result{
				{Name: `suite:a,b,*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Name: `suite:a,b:y;*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
			},
			expect: []result.Result{
				{Name: `suite:*`, Status: result.Fail, Variant: result.Variant{GPU: "X"}},
			},
		},
	} {
		tree, err := tree.New(test.in)
		if err != nil {
			t.Fatalf("Test '%v':\n%v", test.name, err)
		}
		tree.Reduce()
		if diff := deep.Equal(tree.List(), test.expect); diff != nil {
			t.Errorf("Test '%v':\n%v", test.name, diff)
		}
	}
}
