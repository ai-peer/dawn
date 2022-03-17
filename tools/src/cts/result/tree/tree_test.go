package tree_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/cts/result/tree"
	"github.com/go-test/deep"
)

func Q(name string) query.Query {
	tn, err := query.Parse(name)
	if err != nil {
		panic(err)
	}
	return tn
}

func TestNewSingle(t *testing.T) {
	type Test struct {
		in     result.Result
		expect *tree.Tree
	}
	for _, test := range []Test{
		{
			in: result.Result{
				Query:   Q(`suite:*`),
				Status:  result.Pass,
				Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: tree.Nodes{
						{
							Query: query.Query{
								Suite: `suite`,
							},
							Children: tree.Nodes{
								{
									Query: query.Query{
										Suite: `suite`,
										Files: `*`,
									},
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
				Query:   Q(`suite:a,*`),
				Status:  result.Pass,
				Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: tree.Nodes{
						{
							Query: query.Query{
								Suite: `suite`,
							},
							Children: tree.Nodes{
								{
									Query: query.Query{
										Suite: `suite`,
										Files: `a`,
									},
									Children: tree.Nodes{
										{
											Query: query.Query{
												Suite: `suite`,
												Files: `a,*`,
											},
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
				Query:   Q(`suite:a,b:*`),
				Status:  result.Pass,
				Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: tree.Nodes{
						{
							Query: query.Query{
								Suite: `suite`,
							},
							Children: tree.Nodes{
								{
									Query: query.Query{
										Suite: `suite`,
										Files: `a`,
									},
									Children: tree.Nodes{
										{
											Query: query.Query{
												Suite: `suite`,
												Files: `a,b`,
											},
											Children: tree.Nodes{
												{
													Query: query.Query{
														Suite: `suite`,
														Files: `a,b`,
														Tests: `*`,
													},
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
				Query:   Q(`suite:a,b:c:*`),
				Status:  result.Pass,
				Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: tree.Nodes{
						{
							Query: query.Query{
								Suite: `suite`,
							},
							Children: tree.Nodes{
								{
									Query: query.Query{
										Suite: `suite`,
										Files: `a`,
									},
									Children: tree.Nodes{
										{
											Query: query.Query{
												Suite: `suite`,
												Files: `a,b`,
											},
											Children: tree.Nodes{
												{
													Query: query.Query{
														Suite: `suite`,
														Files: `a,b`,
														Tests: `c`,
													},
													Children: tree.Nodes{
														{
															Query: query.Query{
																Suite: `suite`,
																Files: `a,b`,
																Tests: `c`,
																Cases: "*",
															},
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
		{
			in: result.Result{
				Query:   Q(`suite:a,b,c:d,e:f="g";h=[1,2,3];i=4;*`),
				Status:  result.Pass,
				Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: tree.Nodes{
						{
							Query: query.Query{
								Suite: `suite`,
							},
							Children: tree.Nodes{
								{
									Query: query.Query{
										Suite: `suite`,
										Files: `a`,
									},
									Children: tree.Nodes{
										{
											Query: query.Query{
												Suite: `suite`,
												Files: `a,b`,
											},
											Children: tree.Nodes{
												{
													Query: query.Query{
														Suite: `suite`,
														Files: `a,b,c`,
													},
													Children: tree.Nodes{
														{
															Query: query.Query{
																Suite: `suite`,
																Files: `a,b,c`,
																Tests: `d`,
															},
															Children: tree.Nodes{
																{
																	Query: query.Query{
																		Suite: `suite`,
																		Files: `a,b,c`,
																		Tests: `d,e`,
																	},
																	Children: tree.Nodes{
																		{
																			Query: query.Query{
																				Suite: `suite`,
																				Files: `a,b,c`,
																				Tests: `d,e`,
																				Cases: `f="g";h=[1,2,3];i=4;*`,
																			},
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
				Query: Q(`suite:a,b:c:d="e";*`), Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"},
			},
			expect: &tree.Tree{
				Node: tree.Node{
					Children: tree.Nodes{
						{
							Query: query.Query{
								Suite: `suite`,
							},
							Children: tree.Nodes{
								{
									Query: query.Query{
										Suite: `suite`,
										Files: `a`,
									},
									Children: tree.Nodes{
										{
											Query: query.Query{
												Suite: `suite`,
												Files: `a,b`,
											},
											Children: tree.Nodes{
												{
													Query: query.Query{
														Suite: `suite`,
														Files: `a,b`,
														Tests: `c`,
													},
													Children: tree.Nodes{
														{
															Query: query.Query{
																Suite: `suite`,
																Files: `a,b`,
																Tests: `c`,
																Cases: `d="e";*`,
															},
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

		got, err := tree.New(result.List{test.in})
		if err != nil {
			t.Fatalf("New(%v) returned %v", test.in, err)
		}
		if diff := deep.Equal(got, test.expect); diff != nil {
			t.Errorf("New(%v) tree was not as expected:\n%v", test.in, diff)
		}
	}

}

func TestNewMultiple(t *testing.T) {
	got, err := tree.New(result.List{
		{Query: Q(`suite:a,b:c:d="e";*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:h,b:c:f="g";*`), Status: result.Abort, Variant: result.Variant{OS: "A", GPU: "X"}},
		{Query: Q(`suite:a,b:c:f="g";*`), Status: result.Skip, Variant: result.Variant{OS: "A", GPU: "Y"}},
		{Query: Q(`suite:a,b:c:d="e";*`), Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"}},
	})
	if err != nil {
		t.Errorf("New() returned %v", err)
	}
	expect := &tree.Tree{
		Node: tree.Node{
			Children: tree.Nodes{
				{
					Query: query.Query{
						Suite: `suite`,
					},
					Children: tree.Nodes{
						{
							Query: query.Query{
								Suite: `suite`,
								Files: `a`,
							},
							Children: tree.Nodes{
								{
									Query: query.Query{
										Suite: `suite`,
										Files: `a,b`,
									},
									Children: tree.Nodes{
										{
											Query: query.Query{
												Suite: `suite`,
												Files: `a,b`,
												Tests: `c`,
											},
											Children: tree.Nodes{
												{
													Query: query.Query{
														Suite: `suite`,
														Files: `a,b`,
														Tests: `c`,
														Cases: `d="e";*`,
													},
													Results: map[result.Variant]result.Status{
														{OS: "A", GPU: "X"}: result.Pass,
														{OS: "B", GPU: "X"}: result.Fail,
													},
												},
												{
													Query: query.Query{
														Suite: `suite`,
														Files: `a,b`,
														Tests: `c`,
														Cases: `f="g";*`,
													},
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
						{
							Query: query.Query{
								Suite: `suite`,
								Files: `h`,
							},
							Children: tree.Nodes{
								{
									Query: query.Query{
										Suite: `suite`,
										Files: `h,b`,
									},
									Children: tree.Nodes{
										{
											Query: query.Query{
												Suite: `suite`,
												Files: `h,b`,
												Tests: `c`,
											},
											Children: tree.Nodes{
												{
													Query: query.Query{
														Suite: `suite`,
														Files: `h,b`,
														Tests: `c`,
														Cases: `f="g";*`,
													},
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

func TestAllResults(t *testing.T) {
	tree, err := tree.New(result.List{
		{Query: Q(`suite:*`), Status: result.Skip, Variant: result.Variant{OS: "A", GPU: "Y"}},
		{Query: Q(`suite:a,*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:a,b,*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:a,b:c:*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:a,b:c:d;*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:a,b:c:d="e";*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:h,b:c:f="g";*`), Status: result.Abort, Variant: result.Variant{OS: "A", GPU: "X"}},
		{Query: Q(`suite:a,b:c:f="g";*`), Status: result.Skip, Variant: result.Variant{OS: "A", GPU: "Y"}},
		{Query: Q(`suite:a,b:c:d="e";*`), Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"}},
	})
	if err != nil {
		t.Fatalf("New() returned %v", err)
	}
	got := tree.AllResults()
	expect := result.List{
		{Query: Q(`suite:*`), Status: result.Skip, Variant: result.Variant{OS: "A", GPU: "Y"}},
		{Query: Q(`suite:a,*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:a,b:c:*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:a,b:c:d;*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:a,b:c:d="e";*`), Status: result.Pass, Variant: result.Variant{OS: "A", GPU: "X"}},
		{Query: Q(`suite:a,b:c:d="e";*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:a,b:c:f="g";*`), Status: result.Skip, Variant: result.Variant{OS: "A", GPU: "Y"}},
		{Query: Q(`suite:a,b,*`), Status: result.Fail, Variant: result.Variant{OS: "B", GPU: "X"}},
		{Query: Q(`suite:h,b:c:f="g";*`), Status: result.Abort, Variant: result.Variant{OS: "A", GPU: "X"}},
	}
	if diff := deep.Equal(got, expect); diff != nil {
		t.Errorf("AllResults() was not as expected:\n%v", diff)
	}
}

func TestReduce(t *testing.T) {
	type Test struct {
		name   string
		in     result.List
		expect result.List
	}
	for _, test := range []Test{
		{
			name: "Single variant, different results",
			in: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a,c,*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
			expect: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a,c,*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
		},
		{
			name: "Single variant, same results",
			in: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a,c,*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
			},
			expect: result.List{
				{Query: Q(`suite:*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
			},
		},
		{
			name: "Multiple variants, different results",
			in: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a,c,*`), Status: result.Pass, Variant: result.Variant{GPU: "Y"}},
				{Query: Q(`suite:a,d,*`), Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
				{Query: Q(`suite:a,e,*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
			expect: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a,c,*`), Status: result.Pass, Variant: result.Variant{GPU: "Y"}},
				{Query: Q(`suite:a,d,*`), Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
				{Query: Q(`suite:a,e,*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
		},
		{
			name: "Multiple variants, same results",
			in: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a,c,*`), Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
				{Query: Q(`suite:a,d,*`), Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
				{Query: Q(`suite:a,e,*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
			expect: result.List{
				{Query: Q(`suite:*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:*`), Status: result.Fail, Variant: result.Variant{GPU: "Y"}},
			},
		},
		{
			name: "File vs test",
			in: result.List{
				{Query: Q(`suite:a:b,c*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a:b,c*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a,b,c*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a,b,c*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
			expect: result.List{
				{Query: Q(`suite:a:b,*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a,b,*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
		},
		{
			name: "Sibling cases, no reduce",
			in: result.List{
				{Query: Q(`suite:a:b:c;d=e;f=g;*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a:b:c;d=e;f=h;*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
			expect: result.List{
				{Query: Q(`suite:a:b:c;d=e;f=g;*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a:b:c;d=e;f=h;*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
		},
		{
			name: "Sibling cases, reduce to test",
			in: result.List{
				{Query: Q(`suite:a:b:c=1;d="x";*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a:b:c=1;d="y";*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a:z:*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
			expect: result.List{
				{Query: Q(`suite:a:b:*`), Status: result.Fail, Variant: result.Variant{GPU: "X"}},
				{Query: Q(`suite:a:z:*`), Status: result.Pass, Variant: result.Variant{GPU: "X"}},
			},
		},
	} {
		tree, err := tree.New(test.in)
		if err != nil {
			t.Fatalf("Test '%v':\n%v", test.name, err)
		}
		tree.Reduce()
		if diff := deep.Equal(tree.AllResults(), test.expect); diff != nil {
			t.Errorf("Test '%v':\n%v", test.name, diff)
		}
	}
}
