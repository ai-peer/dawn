package common

import (
	"dawn.googlesource.com/dawn/tools/src/utils"
	"go.chromium.org/luci/auth"
	"go.chromium.org/luci/hardcoded/chromeinfra"
)

const (
	// The subject prefix for CTS roll changes
	RollSubjectPrefix = "Roll third_party/webgpu-cts/ "

	// The default directory for the results cache
	DefaultCacheDir = "~/.cache/webgpu-cts-results"
)

func DefaultAuthOptions() auth.Options {
	def := chromeinfra.DefaultAuthOptions()
	def.SecretsDir = utils.ExpandHome("~/.config/dawn-cts")
	return def
}
