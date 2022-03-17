package result

type Status string

const (
	Unknown = Status("Unknown")
	Pass    = Status("Pass")
	Fail    = Status("Fail")
	Crash   = Status("Crash")
	Abort   = Status("Abort")
	Skip    = Status("Skip")
)
