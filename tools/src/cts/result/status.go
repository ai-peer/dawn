package result

type Status string

const (
	Unknown = Status("Unknown")
	Pass    = Status("Pass")
	Failure = Status("Failure")
	Crash   = Status("Crash")
	Abort   = Status("Abort")
	Skip    = Status("Skip")
)
