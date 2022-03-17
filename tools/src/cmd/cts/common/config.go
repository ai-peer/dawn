package common

import (
	"encoding/json"
	"fmt"
	"os"

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

func LoadConfig(path string) (*Config, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, fmt.Errorf("failed to open '%v': %w", path, err)
	}
	defer f.Close()

	cfg := Config{}
	if err := json.NewDecoder(f).Decode(&cfg); err != nil {
		return nil, fmt.Errorf("failed to load config: %w", err)
	}
	return &cfg, nil
}
