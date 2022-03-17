package tree_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/cts/result/tree"
	"github.com/google/go-cmp/cmp"
)

var Q = query.Parse

func TestNewSingle(t *testing.T) {
	type Test struct {
		in     result.Result
		expect *tree.Tree
	}
	for _, test := range []Test{
		{
			in: result.Result{
				Query:  Q(`suite:*`),
				Status: result.Pass,
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
									Status: result.Pass,
								},
							},
						},
					},
				},
			},
		},
		{
			in: result.Result{
				Query:  Q(`suite:a,*`),
				Status: result.Pass,
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
											Status: result.Pass,
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
				Query:  Q(`suite:a,b:*`),
				Status: result.Pass,
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
													Status: result.Pass,
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
				Query:  Q(`suite:a,b:c:*`),
				Status: result.Pass,
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
															Status: result.Pass,
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
				Query:  Q(`suite:a,b,c:d,e:f="g";h=[1,2,3];i=4;*`),
				Status: result.Pass,
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
																			Status: result.Pass,
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
				Query: Q(`suite:a,b:c:d="e";*`), Status: result.Pass,
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
															Status: result.Pass,
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
		if diff := cmp.Diff(got, test.expect); diff != "" {
			t.Errorf("New(%v) tree was not as expected:\n%v", test.in, diff)
		}
	}

}

func TestNewMultiple(t *testing.T) {
	got, err := tree.New(result.List{
		{Query: Q(`suite:a,b:c:d="e";*`), Status: result.Failure},
		{Query: Q(`suite:h,b:c:f="g";*`), Status: result.Abort},
		{Query: Q(`suite:a,b:c:f="g";*`), Status: result.Skip},
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
													Status: result.Failure,
												},
												{
													Query: query.Query{
														Suite: `suite`,
														Files: `a,b`,
														Tests: `c`,
														Cases: `f="g";*`,
													},
													Status: result.Skip,
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
													Status: result.Abort,
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
	if diff := cmp.Diff(got, expect); diff != "" {
		t.Errorf("New() tree was not as expected:\n%v", diff)
		t.Errorf("got:\n%v", got)
		t.Errorf("expect:\n%v", expect)
	}
}

func TestAllResults(t *testing.T) {
	tree, err := tree.New(result.List{
		{Query: Q(`suite:*`), Status: result.Skip},
		{Query: Q(`suite:a,*`), Status: result.Failure},
		{Query: Q(`suite:a,b,*`), Status: result.Failure},
		{Query: Q(`suite:a,b:c:*`), Status: result.Failure},
		{Query: Q(`suite:a,b:c:d;*`), Status: result.Failure},
		{Query: Q(`suite:a,b:c:d="e";*`), Status: result.Failure},
		{Query: Q(`suite:h,b:c:f="g";*`), Status: result.Abort},
		{Query: Q(`suite:a,b:c:f="g";*`), Status: result.Skip},
	})
	if err != nil {
		t.Fatalf("New() returned %v", err)
	}
	got := tree.AllResults(nil)
	expect := result.List{
		{Query: Q(`suite:*`), Status: result.Skip},
		{Query: Q(`suite:a,*`), Status: result.Failure},
		{Query: Q(`suite:a,b:c:*`), Status: result.Failure},
		{Query: Q(`suite:a,b:c:d;*`), Status: result.Failure},
		{Query: Q(`suite:a,b:c:d="e";*`), Status: result.Failure},
		{Query: Q(`suite:a,b:c:f="g";*`), Status: result.Skip},
		{Query: Q(`suite:a,b,*`), Status: result.Failure},
		{Query: Q(`suite:h,b:c:f="g";*`), Status: result.Abort},
	}
	if diff := cmp.Diff(got, expect); diff != "" {
		t.Errorf("AllResults() was not as expected:\n%v", diff)
	}
}

func TestReduce(t *testing.T) {
	type Test struct {
		name   string
		in     result.List
		expect result.List
		skip   bool
	}
	for _, test := range []Test{
		{ //////////////////////////////////////////////////////////////////////
			name: "Different file results",
			in: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Failure},
				{Query: Q(`suite:a,c,*`), Status: result.Pass},
			},
			expect: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Failure},
				{Query: Q(`suite:a,c,*`), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Different test results",
			in: result.List{
				{Query: Q(`suite:a,b:*`), Status: result.Failure},
				{Query: Q(`suite:a,c:*`), Status: result.Pass},
			},
			expect: result.List{
				{Query: Q(`suite:a,b:*`), Status: result.Failure},
				{Query: Q(`suite:a,c:*`), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Same file results",
			in: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Failure},
				{Query: Q(`suite:a,c,*`), Status: result.Failure},
			},
			expect: result.List{
				{Query: Q(`suite:*`), Status: result.Failure},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Same test results",
			in: result.List{
				{Query: Q(`suite:a,b:*`), Status: result.Failure},
				{Query: Q(`suite:a,c:*`), Status: result.Failure},
			},
			expect: result.List{
				{Query: Q(`suite:*`), Status: result.Failure},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "File vs test",
			in: result.List{
				{Query: Q(`suite:a:b,c*`), Status: result.Failure},
				{Query: Q(`suite:a,b,c*`), Status: result.Pass},
			},
			expect: result.List{
				{Query: Q(`suite:a:*`), Status: result.Failure},
				{Query: Q(`suite:a,*`), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Sibling cases, no reduce",
			in: result.List{
				{Query: Q(`suite:a:b:c;d=e;f=g;*`), Status: result.Failure},
				{Query: Q(`suite:a:b:c;d=e;f=h;*`), Status: result.Pass},
			},
			expect: result.List{
				{Query: Q(`suite:a:b:c;d=e;f=g;*`), Status: result.Failure},
				{Query: Q(`suite:a:b:c;d=e;f=h;*`), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Sibling cases, reduce to test",
			in: result.List{
				{Query: Q(`suite:a:b:c=1;d="x";*`), Status: result.Failure},
				{Query: Q(`suite:a:b:c=1;d="y";*`), Status: result.Failure},
				{Query: Q(`suite:a:z:*`), Status: result.Pass},
			},
			expect: result.List{
				{Query: Q(`suite:a:b:*`), Status: result.Failure},
				{Query: Q(`suite:a:z:*`), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Reduce common cases",
			skip: true,
			in: result.List{
				{Query: Q(`suite:a:b:x=1;common="value";*`), Status: result.Failure},
				{Query: Q(`suite:a:b:x=2;common="value";*`), Status: result.Failure},
				{Query: Q(`suite:a:b:x=3;common="value";*`), Status: result.Failure},
				{Query: Q(`suite:a:b:x=4;*`), Status: result.Pass},
			},
			expect: result.List{
				{Query: Q(`suite:a:b:x=1;*`), Status: result.Failure},
				{Query: Q(`suite:a:b:x=2;*`), Status: result.Failure},
				{Query: Q(`suite:a:b:x=3;*`), Status: result.Failure},
				{Query: Q(`suite:a:b:x=4;*`), Status: result.Pass},
			},
		},
	} {
		if test.skip {
			continue
		}
		tree, err := tree.New(test.in)
		if err != nil {
			t.Fatalf("Test '%v':\n%v", test.name, err)
		}
		tree.Reduce()
		results := tree.AllResults(nil)
		if diff := cmp.Diff(results, test.expect); diff != "" {
			t.Errorf("Test '%v':\n%v", test.name, diff)
		}
	}
}

func TestReplace(t *testing.T) {
	type Test struct {
		name   string
		base   result.List
		query  query.Query
		status result.Status
		expect result.List
	}
	for _, test := range []Test{
		{ //////////////////////////////////////////////////////////////////////
			name: "Replace file. Direct",
			base: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Failure},
				{Query: Q(`suite:a,c,*`), Status: result.Pass},
			},
			query:  Q(`suite:a,b,*`),
			status: result.Skip,
			expect: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Skip},
				{Query: Q(`suite:a,c,*`), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Replace file. Indirect",
			base: result.List{
				{Query: Q(`suite:a,b,c,*`), Status: result.Failure},
				{Query: Q(`suite:a,b,d,*`), Status: result.Pass},
				{Query: Q(`suite:a,c,*`), Status: result.Pass},
			},
			query:  Q(`suite:a,b,*`),
			status: result.Skip,
			expect: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Skip},
				{Query: Q(`suite:a,c,*`), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "File vs Test",
			base: result.List{
				{Query: Q(`suite:a,b:c,*`), Status: result.Crash},
				{Query: Q(`suite:a,b:d,*`), Status: result.Abort},
				{Query: Q(`suite:a,b,c,*`), Status: result.Failure},
				{Query: Q(`suite:a,b,d,*`), Status: result.Pass},
			},
			query:  Q(`suite:a,b,*`),
			status: result.Skip,
			expect: result.List{
				{Query: Q(`suite:a,b,*`), Status: result.Skip},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Cases. * with *",
			base: result.List{
				{Query: Q(`suite:file:test:*`), Status: result.Failure},
			},
			query:  Q(`suite:file:test:*`),
			status: result.Pass,
			expect: result.List{
				{Query: Q(`suite:file:test:*`), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Cases. Mixed with *",
			base: result.List{
				{Query: Q(`suite:file:test:a=1,*`), Status: result.Failure},
				{Query: Q(`suite:file:test:a=2,*`), Status: result.Skip},
				{Query: Q(`suite:file:test:a=3,*`), Status: result.Crash},
			},
			query:  Q(`suite:file:test:*`),
			status: result.Pass,
			expect: result.List{
				{Query: Q(`suite:file:test:*`), Status: result.Pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Cases. Replace partial - (a=1)",
			base: result.List{
				{Query: Q(`suite:file:test:a=1;b=x;*`), Status: result.Failure},
				{Query: Q(`suite:file:test:a=1;b=y;*`), Status: result.Failure},
				{Query: Q(`suite:file:test:a=2;b=y;*`), Status: result.Failure},
			},
			query:  Q(`suite:file:test:a=1;*`),
			status: result.Pass,
			expect: result.List{
				{Query: Q(`suite:file:test:a=1;*`), Status: result.Pass},
				{Query: Q(`suite:file:test:a=2;b=y;*`), Status: result.Failure},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Cases. Replace partial - (b=y)",
			base: result.List{
				{Query: Q(`suite:file:test:a=1;b=x;*`), Status: result.Failure},
				{Query: Q(`suite:file:test:a=1;b=y;*`), Status: result.Failure},
				{Query: Q(`suite:file:test:a=2;b=y;*`), Status: result.Failure},
			},
			query:  Q(`suite:file:test:b=y;*`),
			status: result.Pass,
			expect: result.List{
				{Query: Q(`suite:file:test:a=1;b=x;*`), Status: result.Failure},
				{Query: Q(`suite:file:test:b=y;*`), Status: result.Pass},
			},
		},
	} {
		tree, err := tree.New(test.base)
		if err != nil {
			t.Fatalf("Test '%v':\ntree.New() returned %v", test.name, err)
		}
		err = tree.Replace(test.query, test.status)
		if err != nil {
			t.Fatalf("Test '%v':\ntree.Replace() returned %v", test.name, err)
		}
		if diff := cmp.Diff(tree.AllResults(nil), test.expect); diff != "" {
			t.Errorf("Test '%v':\n%v", test.name, diff)
		}
	}
}
