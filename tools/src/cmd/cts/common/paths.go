package common

import (
	"os"
	"path/filepath"

	"dawn.googlesource.com/dawn/tools/src/utils"
)

// DefaultExpectationsPath returns the default path to the expectations.txt
// file. Returns an empty string if the file cannot be found.
func DefaultExpectationsPath() string {
	path := filepath.Join(utils.DawnRoot(), "webgpu-cts/expectations.txt")
	if _, err := os.Stat(path); err != nil {
		return ""
	}
	return path
}
