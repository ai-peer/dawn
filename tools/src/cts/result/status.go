package result

type Status string

const (
	Unknown = Status("UNKNOWN")
	Pass    = Status("PASS")
	Fail    = Status("FAIL")
	Crash   = Status("CRASH")
	Abort   = Status("ABORT")
	Skip    = Status("SKIP")
)
