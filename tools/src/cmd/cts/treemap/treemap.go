// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package treemap

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strings"

	"dawn.googlesource.com/dawn/tools/src/browser"
	"dawn.googlesource.com/dawn/tools/src/cmd/cts/common"
	"dawn.googlesource.com/dawn/tools/src/container"
	"dawn.googlesource.com/dawn/tools/src/cts/query"
	"dawn.googlesource.com/dawn/tools/src/fileutils"
)

func init() {
	common.Register(&cmd{})
}

type cmd struct {
	flags struct {
	}
}

func (cmd) Name() string {
	return "treemap"
}

func (cmd) Desc() string {
	return "displays a treemap visualization of the CTS tests cases"
}

func (c *cmd) RegisterFlags(ctx context.Context, cfg common.Config) ([]string, error) {
	return nil, nil
}

func (c *cmd) Run(ctx context.Context, cfg common.Config) error {
	ctx, stop := context.WithCancel(context.Background())

	handler := http.NewServeMux()
	handler.HandleFunc("/index.html", func(w http.ResponseWriter, r *http.Request) {
		f, err := os.Open(filepath.Join(fileutils.ThisDir(), "treemap.html"))
		if err != nil {
			fmt.Fprint(w, "file not found")
			w.WriteHeader(http.StatusNotFound)
			return
		}
		defer f.Close()
		io.Copy(w, f)
	})
	handler.HandleFunc("/data.json", func(w http.ResponseWriter, r *http.Request) {
		data, err := loadTreeData()
		if err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		io.Copy(w, strings.NewReader(data))
	})
	handler.HandleFunc("/viewer.closed", func(w http.ResponseWriter, r *http.Request) {
		_ = stop
	})

	const port = 9393
	url := fmt.Sprintf("http://localhost:%v/index.html", port)

	server := &http.Server{Addr: fmt.Sprint(":", port), Handler: handler}
	go server.ListenAndServe()

	browser.Open(url)

	<-ctx.Done()
	err := server.Shutdown(ctx)
	switch err {
	case nil, context.Canceled:
		return nil
	default:
		return err
	}
}

func loadTreeData() (string, error) {
	testListPath := filepath.Join(fileutils.DawnRoot(), common.TestListRelPath)

	file, err := os.Open(testListPath)
	if err != nil {
		return "", fmt.Errorf("failed to open test list: %w", err)
	}
	defer file.Close()

	parentOf := func(query string) string {
		if n := strings.LastIndexAny(query, ",:"); n > 0 {
			return query[:n]
		}
		return ""
	}

	queryCounts := container.NewMap[string, int]()

	scanner := bufio.NewScanner(file)
	for scanner.Scan() {
		if name := strings.TrimSpace(scanner.Text()); name != "" {
			q := query.Parse(name)
			q.Cases = "" // Remove query
			for name = q.String(); name != ""; name = parentOf(name) {
				queryCounts[name] = queryCounts[name] + 1
			}
		}
	}

	if err := scanner.Err(); err != nil {
		return "", fmt.Errorf("failed to parse test list: %w", err)
	}

	// https://developers.google.com/chart/interactive/docs/gallery/treemap#data-format
	data := &strings.Builder{}
	fmt.Fprint(data, `[`)
	fmt.Fprint(data, `["Query", "Parent", "Number of tests", "Color"]`)
	for _, query := range queryCounts.Keys() {
		fmt.Fprint(data, ",")

		count := queryCounts[query]

		var row []any
		if parent := parentOf(query); parent != "" {
			row = []any{query, parent, count, count}
		} else {
			row = []any{query, nil, count, count}
		}

		line, err := json.Marshal(row)
		if err != nil {
			return "", err
		}
		fmt.Fprintln(data, string(line))
	}
	fmt.Fprintln(data, `]`)

	return data.String(), nil
}
