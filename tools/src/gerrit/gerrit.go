// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// gerrit provides helpers for obtaining information from Tint's gerrit instance
package gerrit

import (
	"flag"
	"fmt"
	"io/ioutil"
	"net/url"
	"os"
	"regexp"
	"strconv"
	"strings"

	"github.com/andygrunwald/go-gerrit"
)

// Gerrit is the interface to gerrit
type Gerrit struct {
	client        *gerrit.Client
	authenticated bool
}

type Config struct {
	Username string
	Password string
}

// Patchset refers to a single gerrit patchset
type Patchset struct {
	// Gerrit host
	Host string
	// Gerrit project
	Project string
	// Change ID
	Change int
	// Patchset ID
	Patchset int
}

func NewPatchset(change *ChangeInfo) Patchset {
	u, _ := url.Parse(change.URL)
	ps := Patchset{
		Host:     u.Host,
		Project:  change.Project,
		Change:   change.Number,
		Patchset: change.Revisions[change.CurrentRevision].Number,
	}
	return ps
}

type ChangeInfo = gerrit.ChangeInfo

// RegisterFlags registers the command line flags to populate p
func (p *Patchset) RegisterFlags(defaultHost, defaultProject string) {
	flag.StringVar(&p.Host, "host", defaultHost, "gerrit host")
	flag.StringVar(&p.Project, "project", defaultProject, "gerrit project")
	flag.IntVar(&p.Change, "cl", 0, "gerrit change id")
	flag.IntVar(&p.Patchset, "ps", 0, "gerrit patchset id")
}

func LoadCredentials() (user, pass string) {
	cookiesFile := os.Getenv("HOME") + "/.gitcookies"
	if cookies, err := ioutil.ReadFile(cookiesFile); err == nil {
		re := regexp.MustCompile(`dawn-review.googlesource.com\s+(?:FALSE|TRUE)[\s/]+(?:FALSE|TRUE)\s+[0-9]+\s+.\s+(.*)=(.*)`)
		match := re.FindStringSubmatch(string(cookies))
		if len(match) == 3 {
			return match[1], match[2]
		}
	}
	return "", ""
}

func New(url string, cfg Config) (*Gerrit, error) {
	client, err := gerrit.NewClient(url, nil)
	if err != nil {
		return nil, fmt.Errorf("couldn't create gerrit client: %w", err)
	}

	user, pass := cfg.Username, cfg.Password
	if user == "" {
		user, pass = LoadCredentials()
	}

	if user != "" {
		client.Authentication.SetBasicAuth(user, pass)
	}

	return &Gerrit{client, user != ""}, nil
}

func (g *Gerrit) QueryChanges(queryParts ...string) (changes []gerrit.ChangeInfo, query string, err error) {
	changes = []gerrit.ChangeInfo{}
	query = strings.Join(queryParts, "+")
	for {
		batch, _, err := g.client.Changes.QueryChanges(&gerrit.QueryChangeOptions{
			QueryOptions: gerrit.QueryOptions{Query: []string{query}},
			Skip:         len(changes),
		})
		if err != nil {
			return nil, "", g.maybeWrapError(err)
		}

		changes = append(changes, *batch...)
		if len(*batch) == 0 || !(*batch)[len(*batch)-1].MoreChanges {
			break
		}
	}
	return changes, query, nil
}

func (g *Gerrit) Abandon(changeID string) error {
	_, _, err := g.client.Changes.AbandonChange(changeID, &gerrit.AbandonInput{})
	if err != nil {
		return g.maybeWrapError(err)
	}
	return nil
}

func (g *Gerrit) CreateChange(project, branch, subject string, wip bool) (*ChangeInfo, error) {
	change, _, err := g.client.Changes.CreateChange(&gerrit.ChangeInput{
		Project:        project,
		Branch:         branch,
		Subject:        subject,
		WorkInProgress: wip,
	})
	if err != nil {
		return nil, g.maybeWrapError(err)
	}
	return change, nil
}

func (g *Gerrit) EditFiles(changeID, newCommitMsg string, files map[string]string) (Patchset, error) {
	if newCommitMsg != "" {
		resp, err := g.client.Changes.ChangeCommitMessageInChangeEdit(changeID, &gerrit.ChangeEditMessageInput{
			Message: newCommitMsg,
		})
		if err != nil && resp.StatusCode != 409 { // 409 no changes were made
			return Patchset{}, g.maybeWrapError(err)
		}
	}
	for path, content := range files {
		resp, err := g.client.Changes.ChangeFileContentInChangeEdit(changeID, path, content)
		if err != nil && resp.StatusCode != 409 { // 409 no changes were made
			return Patchset{}, g.maybeWrapError(err)
		}
	}

	resp, err := g.client.Changes.PublishChangeEdit(changeID, "NONE")
	if err != nil && resp.StatusCode != 409 { // 409 no changes were made
		return Patchset{}, g.maybeWrapError(err)
	}

	return g.LatestPatchest(changeID)
}

func (g *Gerrit) LatestPatchest(changeID string) (Patchset, error) {
	change, _, err := g.client.Changes.GetChange(changeID, &gerrit.ChangeOptions{
		AdditionalFields: []string{"CURRENT_REVISION"},
	})
	if err != nil {
		return Patchset{}, g.maybeWrapError(err)
	}
	ps := Patchset{
		Host:     g.client.BaseURL().Host,
		Project:  change.Project,
		Change:   change.Number,
		Patchset: change.Revisions[change.CurrentRevision].Number,
	}
	return ps, nil
}

func (g *Gerrit) Comment(ps Patchset, msg string) error {
	_, _, err := g.client.Changes.SetReview(
		strconv.Itoa(ps.Change),
		strconv.Itoa(ps.Patchset),
		&gerrit.ReviewInput{
			Message: msg,
		})
	if err != nil {
		return g.maybeWrapError(err)
	}
	return nil
}
func (g *Gerrit) maybeWrapError(err error) error {
	if err != nil && !g.authenticated {
		return fmt.Errorf(`query failed, possibly because of authentication.
See https://dawn-review.googlesource.com/new-password for obtaining a username
and password which can be provided with --gerrit-user and --gerrit-pass.
%w`, err)
	}
	return err
}
