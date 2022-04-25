package export

import (
	"context"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"dawn.googlesource.com/dawn/tools/src/buildbucket"
	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/cts/result"
	"dawn.googlesource.com/dawn/tools/src/gerrit"
	"dawn.googlesource.com/dawn/tools/src/git"
	"dawn.googlesource.com/dawn/tools/src/gitiles"
	"dawn.googlesource.com/dawn/tools/src/resultsdb"
	"dawn.googlesource.com/dawn/tools/src/utils"
	"go.chromium.org/luci/auth/client/authcli"
	"golang.org/x/oauth2"
	"golang.org/x/oauth2/google"
	"google.golang.org/api/sheets/v4"
)

func init() {
	common.Register(&cmd{})
}

type cmd struct {
	flags struct {
		auth     authcli.Flags
		cacheDir string
	}
}

func (cmd) Name() string {
	return "export"
}

func (cmd) Desc() string {
	return "exports the latest CTS results to Google sheets"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	c.flags.auth.Register(flag.CommandLine, common.DefaultAuthOptions())
	flag.StringVar(&c.flags.cacheDir, "cache", common.DefaultCacheDir, "path to the results cache")
	return nil, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
	// Validate command line arguments
	auth, err := c.flags.auth.Options()
	if err != nil {
		return fmt.Errorf("failed to obtain authentication options: %w", err)
	}

	authdir := utils.ExpandHome(os.ExpandEnv(auth.SecretsDir))
	credentialsPath := filepath.Join(authdir, "credentials.json")
	b, err := ioutil.ReadFile(credentialsPath)
	if err != nil {
		return fmt.Errorf("unable to read credentials file '%v'\n"+
			"Obtain this file from: https://console.developers.google.com/apis/credentials\n%w",
			credentialsPath, err)
	}

	credentials, err := google.CredentialsFromJSON(ctx, b, "https://www.googleapis.com/auth/spreadsheets")
	if err != nil {
		return fmt.Errorf("unable to parse client secret file to config: %w", err)
	}

	s, err := sheets.New(oauth2.NewClient(ctx, credentials.TokenSource))
	if err != nil {
		return fmt.Errorf("unable to create sheets client: %w", err)
	}

	spreadsheet, err := s.Spreadsheets.Get(cfg.Sheets.ID).Do()
	if err != nil {
		return fmt.Errorf("failed to get spreadsheet: %w", err)
	}

	var dataSheet *sheets.Sheet
	for _, sheet := range spreadsheet.Sheets {
		if strings.ToLower(sheet.Properties.Title) == "data" {
			dataSheet = sheet
			break
		}
	}
	if dataSheet == nil {
		return fmt.Errorf("failed to find data sheet")
	}

	columns, err := fetchRow[string](s, spreadsheet, dataSheet, 0)

	gerrit, err := gerrit.New(cfg.Gerrit.Host, gerrit.Credentials{})
	if err != nil {
		return err
	}

	latestRoll, err := common.LatestCTSRoll(gerrit)
	if err != nil {
		return err
	}

	bb, err := buildbucket.New(ctx, auth)
	if err != nil {
		return err
	}

	rdb, err := resultsdb.New(ctx, auth)
	if err != nil {
		return err
	}

	results, ps, err := common.MostRecentResultsForChange(ctx, cfg, c.flags.cacheDir, gerrit, bb, rdb, latestRoll.Number)
	if err != nil {
		return err
	}
	if len(results) == 0 {
		return fmt.Errorf("no results found")
	}

	log.Printf("exporting results from cl %v ps %v...", ps.Change, ps.Patchset)

	dawn, err := gitiles.New(ctx, cfg.Git.Dawn.Host, cfg.Git.Dawn.Project)
	if err != nil {
		return fmt.Errorf("failed to open dawn host: %w", err)
	}

	deps, err := dawn.DownloadFile(ctx, ps.RefsChanges(), "DEPS")
	if err != nil {
		return fmt.Errorf("failed to download DEPS from %v: %w", ps.RefsChanges(), err)
	}

	_, ctsHash, err := common.UpdateCTSHashInDeps(deps, "<unused>")
	if err != nil {
		return fmt.Errorf("failed to find CTS hash in deps: %w", err)
	}

	numUnimplemented, err := countUnimplementedTests(cfg, ctsHash)
	if err != nil {
		return fmt.Errorf("failed to obtain number of unimplemented tests: %w", err)
	}

	counts := map[result.Status]int{}
	for _, r := range results {
		counts[r.Status] = counts[r.Status] + 1
	}

	data := []any{} // First column is change id
	for _, column := range columns {
		switch strings.ToLower(column) {
		case "date":
			data = append(data, time.Now().UTC().Format("2006-01-02"))
		case "change":
			data = append(data, ps.Change)
		case "unimplemented":
			data = append(data, numUnimplemented)
		default:
			count, ok := counts[result.Status(column)]
			if !ok {
				log.Println("no results with status", column)
			}
			data = append(data, count)
		}
	}

	_, err = s.Spreadsheets.Values.Append(spreadsheet.SpreadsheetId, "Data", &sheets.ValueRange{
		Values: [][]any{data},
	}).ValueInputOption("RAW").Do()
	if err != nil {
		return fmt.Errorf("failed to update spreadsheet: %v", err)
	}

	return nil
}

// rowRange returns a sheets range ("name!Ai:i") for the entire row with the
// given index.
func rowRange(index int, sheet *sheets.Sheet) string {
	return fmt.Sprintf("%v!A%v:%v", sheet.Properties.Title, index+1, index+1)
}

// columnRange returns a sheets range ("name!i1:i") for the entire column with
// the given index.
func columnRange(index int, sheet *sheets.Sheet) string {
	col := 'A' + index
	if index > 25 {
		panic("UNIMPLEMENTED")
	}
	return fmt.Sprintf("%v!%c1:%c", sheet.Properties.Title, col, col)
}

// fetchRow returns all the values in the given sheet's row.
func fetchRow[T any](srv *sheets.Service, spreadsheet *sheets.Spreadsheet, sheet *sheets.Sheet, row int) ([]T, error) {
	rng := rowRange(row, sheet)
	data, err := srv.Spreadsheets.Values.Get(spreadsheet.SpreadsheetId, rng).Do()
	if err != nil {
		return nil, fmt.Errorf("Couldn't fetch %v: %w", rng, err)
	}
	out := make([]T, len(data.Values[0]))
	for column, v := range data.Values[0] {
		val, ok := v.(T)
		if !ok {
			return nil, fmt.Errorf("cell at %v:%v was type %T, but expected type %T", row, column, v, val)
		}
		out[column] = val
	}
	return out, nil
}

// fetchColumn returns all the values in the given sheet's column.
func fetchColumn(srv *sheets.Service, spreadsheet *sheets.Spreadsheet, sheet *sheets.Sheet, row int) ([]interface{}, error) {
	rng := columnRange(row, sheet)
	data, err := srv.Spreadsheets.Values.Get(spreadsheet.SpreadsheetId, rng).Do()
	if err != nil {
		return nil, fmt.Errorf("Couldn't fetch %v: %w", rng, err)
	}
	out := make([]interface{}, len(data.Values))
	for i, l := range data.Values {
		if len(l) > 0 {
			out[i] = l[0]
		}
	}
	return out, nil
}

func countUnimplementedTests(cfg common.Config, ctsHash string) (int, error) {
	tmpDir, err := os.MkdirTemp("", "dawn-cts-export")
	if err != nil {
		return 0, err
	}
	defer os.RemoveAll(tmpDir)

	dir := filepath.Join(tmpDir, "cts")

	gitExe, err := exec.LookPath("git")
	if err != nil {
		return 0, fmt.Errorf("failed to find git on PATH: %w", err)
	}

	git, err := git.New(gitExe)
	if err != nil {
		return 0, err
	}

	log.Printf("cloning cts to '%v'...", dir)
	repo, err := git.Clone(dir, cfg.Git.CTS.HttpsURL(), nil)
	if err != nil {
		return 0, fmt.Errorf("failed to clone cts: %v", err)
	}

	log.Printf("checking out cts @ '%v'...", ctsHash)
	if _, err := repo.Fetch(ctsHash, nil); err != nil {
		return 0, fmt.Errorf("failed to fetch cts: %v", err)
	}
	if err := repo.Checkout(ctsHash, nil); err != nil {
		return 0, fmt.Errorf("failed to clone cts: %v", err)
	}

	{
		npm, err := exec.LookPath("npm")
		if err != nil {
			return 0, fmt.Errorf("failed to find npm on PATH: %w", err)
		}
		cmd := exec.Command(npm, "install")
		cmd.Dir = dir
		out, err := cmd.CombinedOutput()
		if err != nil {
			return 0, fmt.Errorf("failed to run npm install: %w\n%v", err, string(out))
		}
	}
	{
		npx, err := exec.LookPath("npx")
		if err != nil {
			return 0, fmt.Errorf("failed to find npx on PATH: %w", err)
		}
		cmd := exec.Command(npx, "grunt", "run:build-out-node")
		cmd.Dir = dir
		out, err := cmd.CombinedOutput()
		if err != nil {
			return 0, fmt.Errorf("failed to build CTS typescript: %w\n%v", err, string(out))
		}
	}
	{
		sh, err := exec.LookPath("sh")
		if err != nil {
			return 0, fmt.Errorf("failed to find sh on PATH: %w", err)
		}
		cmd := exec.Command(sh, "./tools/run_node", "--list-unimplemented", "webgpu:*")
		cmd.Dir = dir
		out, err := cmd.CombinedOutput()
		if err != nil {
			return 0, fmt.Errorf("failed to build CTS typescript: %w", err)
		}
		lines := strings.Split(string(out), "\n")
		for _, line := range lines {
			fmt.Println(line)
		}
		panic(len(lines))
		return len(lines), nil
	}
}
