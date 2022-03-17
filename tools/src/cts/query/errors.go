package query

import "fmt"

type ErrNoDataForQuery struct {
	Query Query
}

func (e ErrNoDataForQuery) Error() string {
	return fmt.Sprintf("no data for query '%v'", e.Query)
}

type ErrDuplicateData struct {
	Query Query
}

func (e ErrDuplicateData) Error() string {
	return fmt.Sprintf("duplicate data '%v'", e.Query)
}
