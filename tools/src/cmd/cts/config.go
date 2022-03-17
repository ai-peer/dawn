package main

import (
	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cts/expectations"
)

type GitProject struct {
	Host    string
	Project string
}

type Config struct {
	TestFilter    string
	TestNameRegex string
	Gerrit        struct {
		Host    string
		Project string
	}
	Git struct {
		CTS  GitProject
		Dawn GitProject
	}
	Expectations expectations.Config
	Builders     map[string]buildbucket.Builder
}

func (c *Config) Validate() error {
	if err := c.Expectations.Validate(); err != nil {
		return err
	}
	return nil
}
