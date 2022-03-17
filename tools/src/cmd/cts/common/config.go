package common

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"time"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"github.com/tidwall/jsonc"
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
		Prefix        string
		SlowThreshold time.Duration
	}
	Gerrit struct {
		Host    string
		Project string
	}
	Git struct {
		CTS  GitProject
		Dawn GitProject
	}
	Builders   map[string]buildbucket.Builder
	TagAliases [][]string
	Sheets     struct {
		ID string
	}
}

func LoadConfig(path string) (*Config, error) {
	data, err := ioutil.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to open '%v': %w", path, err)
	}

	// Remove comments, trailing commas
	data = jsonc.ToJSONInPlace(data)

	cfg := Config{}
	if err := json.NewDecoder(bytes.NewReader(data)).Decode(&cfg); err != nil {
		return nil, fmt.Errorf("failed to load config: %w", err)
	}
	return &cfg, nil
}
