package query_test

import (
	"testing"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"github.com/google/go-cmp/cmp"
)

var (
	abort   = "Abort"
	crash   = "Crash"
	failure = "Failure"
	pass    = "Pass"
	skip    = "Skip"
)

func NewTree[Data any](t *testing.T, entries ...query.QueryData[Data]) query.Tree[Data] {
	t.Helper()
	out, err := query.NewTree(entries...)
	if err != nil {
		t.Fatalf("New() returned %v", err)
	}
	return out
}

func TestNewSingle(t *testing.T) {
	type Tree = query.Tree[string]
	type Node = query.TreeNode[string]
	type QueryData = query.QueryData[string]
	type Children = query.TreeNodeChildren[string]

	type Test struct {
		in     QueryData
		expect Tree
	}
	for _, test := range []Test{
		{ /////////////////////////////////////////////////////////////////////
			in: QueryData{
				Query: Q(`suite:*`),
				Data:  pass,
			},
			expect: Tree{
				TreeNode: Node{
					Children: Children{
						query.TreeNodeChildKey{`suite`, query.Suite}: {
							Query: Q(`suite`),
							Children: Children{
								query.TreeNodeChildKey{`*`, query.Files}: {
									Query: Q(`suite:*`),
									Data:  &pass,
								},
							},
						},
					},
				},
			},
		},
		{ /////////////////////////////////////////////////////////////////////
			in: QueryData{
				Query: Q(`suite:a,*`),
				Data:  pass,
			},
			expect: Tree{
				TreeNode: Node{
					Children: Children{
						query.TreeNodeChildKey{`suite`, query.Suite}: {
							Query: Q(`suite`),
							Children: Children{
								query.TreeNodeChildKey{`a`, query.Files}: {
									Query: Q(`suite:a`),
									Children: Children{
										query.TreeNodeChildKey{`*`, query.Files}: {
											Query: Q(`suite:a,*`),
											Data:  &pass,
										},
									},
								},
							},
						},
					},
				},
			},
		},
		{ /////////////////////////////////////////////////////////////////////
			in: QueryData{
				Query: Q(`suite:a,b:*`),
				Data:  pass,
			},
			expect: Tree{
				TreeNode: Node{
					Children: Children{
						query.TreeNodeChildKey{`suite`, query.Suite}: {
							Query: Q(`suite`),
							Children: Children{
								query.TreeNodeChildKey{`a`, query.Files}: {
									Query: Q(`suite:a`),
									Children: Children{
										query.TreeNodeChildKey{`b`, query.Files}: {
											Query: Q(`suite:a,b`),
											Children: Children{
												query.TreeNodeChildKey{`*`, query.Tests}: {
													Query: Q(`suite:a,b:*`),
													Data:  &pass,
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
		{ /////////////////////////////////////////////////////////////////////
			in: QueryData{
				Query: Q(`suite:a,b:c:*`),
				Data:  pass,
			},
			expect: Tree{
				TreeNode: Node{
					Children: Children{
						query.TreeNodeChildKey{`suite`, query.Suite}: {
							Query: Q(`suite`),
							Children: Children{
								query.TreeNodeChildKey{`a`, query.Files}: {
									Query: Q(`suite:a`),
									Children: Children{
										query.TreeNodeChildKey{`b`, query.Files}: {
											Query: Q(`suite:a,b`),
											Children: Children{
												query.TreeNodeChildKey{`c`, query.Tests}: {
													Query: Q(`suite:a,b:c`),
													Children: Children{
														query.TreeNodeChildKey{`*`, query.Cases}: {
															Query: Q(`suite:a,b:c:*`),
															Data:  &pass,
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
		{ /////////////////////////////////////////////////////////////////////
			in: QueryData{
				Query: Q(`suite:a,b,c:d,e:f="g";h=[1,2,3];i=4;*`),
				Data:  pass,
			},
			expect: Tree{
				TreeNode: Node{
					Children: Children{
						query.TreeNodeChildKey{`suite`, query.Suite}: {
							Query: Q(`suite`),
							Children: Children{
								query.TreeNodeChildKey{`a`, query.Files}: {
									Query: Q(`suite:a`),
									Children: Children{
										query.TreeNodeChildKey{`b`, query.Files}: {
											Query: Q(`suite:a,b`),
											Children: Children{
												query.TreeNodeChildKey{`c`, query.Files}: {
													Query: Q(`suite:a,b,c`),
													Children: Children{
														query.TreeNodeChildKey{`d`, query.Tests}: {
															Query: Q(`suite:a,b,c:d`),
															Children: Children{
																query.TreeNodeChildKey{`e`, query.Tests}: {
																	Query: Q(`suite:a,b,c:d,e`),
																	Children: Children{
																		query.TreeNodeChildKey{`f="g";h=[1,2,3];i=4;*`, query.Cases}: {
																			Query: Q(`suite:a,b,c:d,e:f="g";h=[1,2,3];i=4;*`),
																			Data:  &pass,
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
		{ /////////////////////////////////////////////////////////////////////
			in: QueryData{
				Query: Q(`suite:a,b:c:d="e";*`), Data: pass,
			},
			expect: Tree{
				TreeNode: Node{
					Children: Children{
						query.TreeNodeChildKey{`suite`, query.Suite}: {
							Query: Q(`suite`),
							Children: Children{
								query.TreeNodeChildKey{`a`, query.Files}: {
									Query: Q(`suite:a`),
									Children: Children{
										query.TreeNodeChildKey{`b`, query.Files}: {
											Query: Q(`suite:a,b`),
											Children: Children{
												query.TreeNodeChildKey{`c`, query.Tests}: {
													Query: Q(`suite:a,b:c`),
													Children: Children{
														query.TreeNodeChildKey{`d="e";*`, query.Cases}: {
															Query: Q(`suite:a,b:c:d="e";*`),
															Data:  &pass,
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
		got := NewTree(t, test.in)
		if diff := cmp.Diff(got, test.expect); diff != "" {
			t.Errorf("New(%v) tree was not as expected:\n%v", test.in, diff)
		}
	}

}

func TestNewMultiple(t *testing.T) {
	type Tree = query.Tree[string]
	type Node = query.TreeNode[string]
	type QueryData = query.QueryData[string]
	type Children = query.TreeNodeChildren[string]

	got := NewTree(t,
		QueryData{Query: Q(`suite:a,b:c:d="e";*`), Data: failure},
		QueryData{Query: Q(`suite:h,b:c:f="g";*`), Data: abort},
		QueryData{Query: Q(`suite:a,b:c:f="g";*`), Data: skip},
	)

	expect := Tree{
		TreeNode: Node{
			Children: Children{
				query.TreeNodeChildKey{`suite`, query.Suite}: {
					Query: Q(`suite`),
					Children: Children{
						query.TreeNodeChildKey{`a`, query.Files}: {
							Query: Q(`suite:a`),
							Children: Children{
								query.TreeNodeChildKey{`b`, query.Files}: {
									Query: Q(`suite:a,b`),
									Children: Children{
										query.TreeNodeChildKey{`c`, query.Tests}: {
											Query: Q(`suite:a,b:c`),
											Children: Children{
												query.TreeNodeChildKey{`d="e";*`, query.Cases}: {
													Query: Q(`suite:a,b:c:d="e";*`),
													Data:  &failure,
												},
												query.TreeNodeChildKey{`f="g";*`, query.Cases}: {
													Query: Q(`suite:a,b:c:f="g";*`),
													Data:  &skip,
												},
											},
										},
									},
								},
							},
						},
						query.TreeNodeChildKey{`h`, query.Files}: {
							Query: query.Query{
								Suite: `suite`,
								Files: `h`,
							},
							Children: Children{
								query.TreeNodeChildKey{`b`, query.Files}: {
									Query: query.Query{
										Suite: `suite`,
										Files: `h,b`,
									},
									Children: Children{
										query.TreeNodeChildKey{`c`, query.Tests}: {
											Query: query.Query{
												Suite: `suite`,
												Files: `h,b`,
												Tests: `c`,
											},
											Children: Children{
												query.TreeNodeChildKey{`f="g";*`, query.Cases}: {
													Query: query.Query{
														Suite: `suite`,
														Files: `h,b`,
														Tests: `c`,
														Cases: `f="g";*`,
													},
													Data: &abort,
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

func TestList(t *testing.T) {
	type QueryData = query.QueryData[string]

	tree := NewTree(t,
		QueryData{Query: Q(`suite:*`), Data: skip},
		QueryData{Query: Q(`suite:a,*`), Data: failure},
		QueryData{Query: Q(`suite:a,b,*`), Data: failure},
		QueryData{Query: Q(`suite:a,b:c:*`), Data: failure},
		QueryData{Query: Q(`suite:a,b:c:d;*`), Data: failure},
		QueryData{Query: Q(`suite:a,b:c:d="e";*`), Data: failure},
		QueryData{Query: Q(`suite:h,b:c:f="g";*`), Data: abort},
		QueryData{Query: Q(`suite:a,b:c:f="g";*`), Data: skip},
	)
	got := tree.List()
	expect := []QueryData{
		{Query: Q(`suite:*`), Data: skip},
		{Query: Q(`suite:a,*`), Data: failure},
		{Query: Q(`suite:a,b,*`), Data: failure},
		{Query: Q(`suite:a,b:c:*`), Data: failure},
		{Query: Q(`suite:a,b:c:d;*`), Data: failure},
		{Query: Q(`suite:a,b:c:d="e";*`), Data: failure},
		{Query: Q(`suite:a,b:c:f="g";*`), Data: skip},
		{Query: Q(`suite:h,b:c:f="g";*`), Data: abort},
	}
	if diff := cmp.Diff(got, expect); diff != "" {
		t.Errorf("List() was not as expected:\n%v", diff)
	}
}

func TestReduce(t *testing.T) {
	type QueryData = query.QueryData[string]

	type Test struct {
		name   string
		in     []QueryData
		expect []QueryData
	}
	for _, test := range []Test{
		{ //////////////////////////////////////////////////////////////////////
			name: "Different file results",
			in: []QueryData{
				{Query: Q(`suite:a,b,*`), Data: failure},
				{Query: Q(`suite:a,c,*`), Data: pass},
			},
			expect: []QueryData{
				{Query: Q(`suite:a,b,*`), Data: failure},
				{Query: Q(`suite:a,c,*`), Data: pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Different test results",
			in: []QueryData{
				{Query: Q(`suite:a,b:*`), Data: failure},
				{Query: Q(`suite:a,c:*`), Data: pass},
			},
			expect: []QueryData{
				{Query: Q(`suite:a,b:*`), Data: failure},
				{Query: Q(`suite:a,c:*`), Data: pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Same file results",
			in: []QueryData{
				{Query: Q(`suite:a,b,*`), Data: failure},
				{Query: Q(`suite:a,c,*`), Data: failure},
			},
			expect: []QueryData{
				{Query: Q(`suite:*`), Data: failure},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Same test results",
			in: []QueryData{
				{Query: Q(`suite:a,b:*`), Data: failure},
				{Query: Q(`suite:a,c:*`), Data: failure},
			},
			expect: []QueryData{
				{Query: Q(`suite:*`), Data: failure},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "File vs test",
			in: []QueryData{
				{Query: Q(`suite:a:b,c*`), Data: failure},
				{Query: Q(`suite:a,b,c*`), Data: pass},
			},
			expect: []QueryData{
				{Query: Q(`suite:a,*`), Data: pass},
				{Query: Q(`suite:a:*`), Data: failure},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Sibling cases, no reduce",
			in: []QueryData{
				{Query: Q(`suite:a:b:c;d=e;f=g;*`), Data: failure},
				{Query: Q(`suite:a:b:c;d=e;f=h;*`), Data: pass},
			},
			expect: []QueryData{
				{Query: Q(`suite:a:b:c;d=e;f=g;*`), Data: failure},
				{Query: Q(`suite:a:b:c;d=e;f=h;*`), Data: pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Sibling cases, reduce to test",
			in: []QueryData{
				{Query: Q(`suite:a:b:c=1;d="x";*`), Data: failure},
				{Query: Q(`suite:a:b:c=1;d="y";*`), Data: failure},
				{Query: Q(`suite:a:z:*`), Data: pass},
			},
			expect: []QueryData{
				{Query: Q(`suite:a:b:*`), Data: failure},
				{Query: Q(`suite:a:z:*`), Data: pass},
			},
		},
	} {
		tree := NewTree(t, test.in...)
		tree.Reduce(func(statuses []string) *string {
			first := statuses[0]
			for _, s := range statuses[1:] {
				if first != s {
					return nil
				}
			}
			return &first
		})
		results := tree.List()
		if diff := cmp.Diff(results, test.expect); diff != "" {
			t.Errorf("Test '%v':\n%v", test.name, diff)
		}
	}
}

func TestReplace(t *testing.T) {
	type QueryData = query.QueryData[string]

	type Test struct {
		name        string
		base        []QueryData
		replacement QueryData
		expect      []QueryData
	}
	for _, test := range []Test{
		{ //////////////////////////////////////////////////////////////////////
			name: "Replace file. Direct",
			base: []QueryData{
				{Query: Q(`suite:a,b,*`), Data: failure},
				{Query: Q(`suite:a,c,*`), Data: pass},
			},
			replacement: QueryData{Q(`suite:a,b,*`), skip},
			expect: []QueryData{
				{Query: Q(`suite:a,b,*`), Data: skip},
				{Query: Q(`suite:a,c,*`), Data: pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Replace file. Indirect",
			base: []QueryData{
				{Query: Q(`suite:a,b,c,*`), Data: failure},
				{Query: Q(`suite:a,b,d,*`), Data: pass},
				{Query: Q(`suite:a,c,*`), Data: pass},
			},
			replacement: QueryData{Q(`suite:a,b,*`), skip},
			expect: []QueryData{
				{Query: Q(`suite:a,b,*`), Data: skip},
				{Query: Q(`suite:a,c,*`), Data: pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "File vs Test",
			base: []QueryData{
				{Query: Q(`suite:a,b:c,*`), Data: crash},
				{Query: Q(`suite:a,b:d,*`), Data: abort},
				{Query: Q(`suite:a,b,c,*`), Data: failure},
				{Query: Q(`suite:a,b,d,*`), Data: pass},
			},
			replacement: QueryData{Q(`suite:a,b,*`), skip},
			expect: []QueryData{
				{Query: Q(`suite:a,b,*`), Data: skip},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Cases. * with *",
			base: []QueryData{
				{Query: Q(`suite:file:test:*`), Data: failure},
			},
			replacement: QueryData{Q(`suite:file:test:*`), pass},
			expect: []QueryData{
				{Query: Q(`suite:file:test:*`), Data: pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Cases. Mixed with *",
			base: []QueryData{
				{Query: Q(`suite:file:test:a=1,*`), Data: failure},
				{Query: Q(`suite:file:test:a=2,*`), Data: skip},
				{Query: Q(`suite:file:test:a=3,*`), Data: crash},
			},
			replacement: QueryData{Q(`suite:file:test:*`), pass},
			expect: []QueryData{
				{Query: Q(`suite:file:test:*`), Data: pass},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Cases. Replace partial - (a=1)",
			base: []QueryData{
				{Query: Q(`suite:file:test:a=1;b=x;*`), Data: failure},
				{Query: Q(`suite:file:test:a=1;b=y;*`), Data: failure},
				{Query: Q(`suite:file:test:a=2;b=y;*`), Data: failure},
			},
			replacement: QueryData{Q(`suite:file:test:a=1;*`), pass},
			expect: []QueryData{
				{Query: Q(`suite:file:test:a=1;*`), Data: pass},
				{Query: Q(`suite:file:test:a=2;b=y;*`), Data: failure},
			},
		},
		{ //////////////////////////////////////////////////////////////////////
			name: "Cases. Replace partial - (b=y)",
			base: []QueryData{
				{Query: Q(`suite:file:test:a=1;b=x;*`), Data: failure},
				{Query: Q(`suite:file:test:a=1;b=y;*`), Data: failure},
				{Query: Q(`suite:file:test:a=2;b=y;*`), Data: failure},
			},
			replacement: QueryData{Q(`suite:file:test:b=y;*`), pass},
			expect: []QueryData{
				{Query: Q(`suite:file:test:a=1;b=x;*`), Data: failure},
				{Query: Q(`suite:file:test:b=y;*`), Data: pass},
			},
		},
	} {
		tree := NewTree(t, test.base...)
		err := tree.Replace(test.replacement.Query, test.replacement.Data)
		if err != nil {
			t.Fatalf("Test '%v':\ntree.Replace() returned %v", test.name, err)
		}
		if diff := cmp.Diff(tree.List(), test.expect); diff != "" {
			t.Errorf("Test '%v':\n%v", test.name, diff)
		}
	}
}
