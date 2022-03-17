package result

type Status string

const (
	Abort          = Status("Abort")
	Crash          = Status("Crash")
	Failure        = Status("Failure")
	Pass           = Status("Pass")
	RetryOnFailure = Status("RetryOnFailure")
	Skip           = Status("Skip")
	Slow           = Status("Slow")
	Unknown        = Status("Unknown")
)
