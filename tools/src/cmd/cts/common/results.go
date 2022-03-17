package common

import (
	"context"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/resultsdb"
	rdbpb "go.chromium.org/luci/resultdb/proto/v1"
)

func GetResults(
	ctx context.Context,
	cfg Config,
	rdb *resultsdb.ResultsDB,
	builds BuildsByName) (result.List, error) {

	results := result.List{}
	err := rdb.QueryTestResults(ctx, builds.ids(), cfg.Test.Prefix+".*", func(rpb *rdbpb.TestResult) error {
		if !strings.HasPrefix(rpb.GetTestId(), cfg.Test.Prefix) {
			return nil
		}

		testName := rpb.GetTestId()[len(cfg.Test.Prefix):]
		status := toStatus(rpb.Status)
		tags := result.NewTags()

		for _, sp := range rpb.Tags {
			if sp.Key == "typ_tag" {
				tags.Add(sp.Value)
			}
		}

		if fr := rpb.GetFailureReason(); fr != nil {
			if strings.Contains(fr.GetPrimaryErrorMessage(), "asyncio.exceptions.TimeoutError") {
				status = result.Slow
			}
		}

		results = append(results, result.Result{
			Query:  query.Parse(testName),
			Status: status,
			Tags:   tags,
		})
		return nil
	})

	if err != nil {
		return nil, err
	}

	results.Sort()
	return results, err
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
