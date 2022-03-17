package utils

import (
	"path/filepath"
	"runtime"
)

// ThisDir returns the directory of the caller function
func ThisDir() string {
	_, file, _, ok := runtime.Caller(1)
	if !ok {
		return ""
	}
	return filepath.Dir(file)
}
