package gerrit

import (
	"context"
	"flag"

	"go.chromium.org/luci/auth"
	gerrit "go.chromium.org/luci/common/api/gerrit"
	gpb "go.chromium.org/luci/common/proto/gerrit"
)

type ChangeID int
type PatchsetID int

// Patchset refers to a single gerrit patchset
type Patchset struct {
	// Gerrit host
	Host string
	// Gerrit project
	Project string
	// Change ID
	Change ChangeID
	// Patchset ID
	Patchset PatchsetID
}

// RegisterFlags registers the command line flags to populate p
func (p *Patchset) RegisterFlags(defaultHost, defaultProject string) {
	flag.StringVar(&p.Host, "host", defaultHost, "gerrit host")
	flag.StringVar(&p.Project, "project", defaultProject, "gerrit project")
	flag.IntVar((*int)(&p.Change), "cl", 0, "gerrit change id")
	flag.IntVar((*int)(&p.Patchset), "ps", 0, "gerrit patchset id")
}

// Gerrit is the client to communicate with Gerrit.
type Gerrit struct {
	client  gpb.GerritClient
	project string
}

// New creates a client to communicate with Gerrit.
func New(ctx context.Context, host, project string, credentials auth.Options) (*Gerrit, error) {
	credentials.Scopes = append(credentials.Scopes, gerrit.OAuthScope)
	http, err := auth.NewAuthenticator(ctx, auth.InteractiveLogin, credentials).Client()
	if err != nil {
		return nil, err
	}
	client, err := gerrit.NewRESTClient(http, host, true)
	if err != nil {
		return nil, err
	}
	return &Gerrit{client, project}, nil
}

func (g *Gerrit) CreateChange(ctx context.Context, parent, subject string) (ChangeID, error) {
	res, err := g.client.CreateChange(ctx, &gpb.CreateChangeRequest{
		Project:    g.project,
		Ref:        parent,
		Subject:    subject,
		BaseCommit: parent,
	})
	if err != nil {
		return 0, err
	}
	return ChangeID(res.Number), nil
}

func (g *Gerrit) ListChanges(ctx context.Context, query string) ([]*gpb.ChangeInfo, error) {
	res, err := g.client.ListChanges(ctx, &gpb.ListChangesRequest{
		Query: query,
		// Limit: 10,
	})
	if err != nil {
		return nil, err
	}
	return res.GetChanges(), nil
}

func (g *Gerrit) EditFile(ctx context.Context, change ChangeID, path, content string) error {
	_, err := g.client.ChangeEditFileContent(ctx, &gpb.ChangeEditFileContentRequest{
		Number:   int64(change),
		Project:  g.project,
		FilePath: path,
		Content:  []byte(content),
	})
	return err
}

func (g *Gerrit) Abandon(ctx context.Context, change ChangeID) error {
	_, err := g.client.AbandonChange(ctx, &gpb.AbandonChangeRequest{
		Number:  int64(change),
		Project: g.project,
	})
	return err
}
