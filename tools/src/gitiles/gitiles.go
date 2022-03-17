package gitiles

import (
	"context"
	"fmt"
	"net/http"

	"go.chromium.org/luci/common/api/gitiles"
	gpb "go.chromium.org/luci/common/proto/gitiles"
)

// Gitiles is the client to communicate with Gitiles.
type Gitiles struct {
	client  gpb.GitilesClient
	project string
}

// New creates a client to communicate with Gitiles.
func New(ctx context.Context, host, project string) (*Gitiles, error) {
	client, err := gitiles.NewRESTClient(http.DefaultClient, host, false)
	if err != nil {
		return nil, err
	}
	return &Gitiles{client, project}, nil
}

func (g *Gitiles) Hash(ctx context.Context, ref string) (string, error) {
	res, err := g.client.Log(ctx, &gpb.LogRequest{
		Project:    g.project,
		Committish: ref,
		PageSize:   1,
	})
	if err != nil {
		return "", err
	}
	log := res.GetLog()
	if len(log) == 0 {
		return "", fmt.Errorf("gitiles returned log was empty")
	}
	return log[0].Id, nil
}

func (g *Gitiles) DownloadFile(ctx context.Context, ref, path string) (string, error) {
	res, err := g.client.DownloadFile(ctx, &gpb.DownloadFileRequest{
		Project:    g.project,
		Committish: ref,
		Path:       path,
	})
	if err != nil {
		return "", err
	}
	return res.GetContents(), nil
}
