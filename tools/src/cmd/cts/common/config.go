package common

import (
	"fmt"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
)

type GitProject struct {
	Host    string
	Project string
}

func (g GitProject) HttpsURL() string {
	return fmt.Sprintf("https://%v/%v", g.Host, g.Project)
}

type Config struct {
	Test struct {
		Prefix string
	}
	Gerrit struct {
		Host    string
		Project string
	}
	Git struct {
		CTS  GitProject
		Dawn GitProject
	}
	Builders map[string]buildbucket.Builder
}
