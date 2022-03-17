package result

// Result holds the result of a CTS test
type Result struct {
	Name    string
	Status  Status
	Variant Variant
}

// List is a list of results
type List []Result

// ReplaceDuplicates returns a new list with duplicate test results replaced.
// When a duplicate is found, the function f is called with the duplicate
// results. The returned status will be used as the replaced result.
func (l List) ReplaceDuplicates(f func(List) Status) List {
	type key struct {
		name    string
		variant Variant
	}
	m := map[key]List{}
	for _, r := range l {
		k := key{r.Name, r.Variant}
		m[k] = append(m[k], r)
	}
	for key, results := range m {
		if len(results) > 1 {
			result := results[0]
			result.Status = f(results)
			m[key] = List{result}
		}
	}
	out := make(List, 0, len(m))
	for _, r := range l {
		k := key{r.Name, r.Variant}
		if unique, ok := m[k]; ok {
			out = append(out, unique[0])
			delete(m, k)
		}
	}
	return out
}
