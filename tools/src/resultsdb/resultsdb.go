package resultsdb

import (
	"context"
	"fmt"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"go.chromium.org/luci/auth"
	"go.chromium.org/luci/grpc/prpc"
	"go.chromium.org/luci/hardcoded/chromeinfra"
	rdbpb "go.chromium.org/luci/resultdb/proto/v1"
	"google.golang.org/protobuf/types/known/fieldmaskpb"
)

// ResultsDB is the client to communicate with ResultDB.
type ResultsDB struct {
	client rdbpb.ResultDBClient
}

// New creates a client to communicate with ResultDB.
func New(ctx context.Context, credentials auth.Options) (*ResultsDB, error) {
	http, err := auth.NewAuthenticator(ctx, auth.InteractiveLogin, credentials).Client()
	if err != nil {
		return nil, err
	}
	client, err := rdbpb.NewResultDBPRPCClient(
		&prpc.Client{
			C:       http,
			Host:    chromeinfra.ResultDBHost,
			Options: prpc.DefaultOptions(),
		}), nil
	if err != nil {
		return nil, err
	}

	return &ResultsDB{client}, nil
}

func toStatus(s rdbpb.TestStatus) result.Status {
	switch s {
	default:
		return result.Unknown
	case rdbpb.TestStatus_PASS:
		return result.Pass
	case rdbpb.TestStatus_FAIL:
		return result.Failure
	case rdbpb.TestStatus_CRASH:
		return result.Crash
	case rdbpb.TestStatus_ABORT:
		return result.Abort
	case rdbpb.TestStatus_SKIP:
		return result.Skip
	}
}

// QueryTestResults queries test variants with any unexpected result.
// f is called once per page of test variants.
func (r *ResultsDB) QueryTestResults(ctx context.Context, builds []buildbucket.BuildID, filterRegex string, f func(*rdbpb.TestResult) error) error {
	invocationNames := make([]string, len(builds))
	for i, id := range builds {
		invocationNames[i] = fmt.Sprintf("invocations/build-%v", id)
	}

	pageToken := ""
	for {
		rsp, err := r.client.QueryTestResults(ctx, &rdbpb.QueryTestResultsRequest{
			Invocations: invocationNames,
			Predicate: &rdbpb.TestResultPredicate{
				TestIdRegexp: filterRegex,
			},
			ReadMask: &fieldmaskpb.FieldMask{Paths: []string{
				"status", "tags", "failure_reason",
			}},
			PageSize:  1000, // Maximum page size.
			PageToken: pageToken,
		})
		if err != nil {
			return err
		}

		for _, res := range rsp.TestResults {
			if err := f(res); err != nil {
				return err
			}
		}

		pageToken = rsp.GetNextPageToken()
		if pageToken == "" {
			break
		}
	}

	return nil
}
