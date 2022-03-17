package utils

import (
	"crypto/sha256"
	"fmt"
)

func Hash(values ...interface{}) string {
	h := sha256.New()
	for _, v := range values {
		h.Write([]byte(fmt.Sprintf("%v", v)))
		h.Write([]byte{0})
	}
	return fmt.Sprintf("%x", h.Sum(nil))
}
