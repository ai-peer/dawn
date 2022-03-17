package multicmd

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"net/http"
	"net/http/pprof"
	"os"
	"path/filepath"
	"strings"
	"text/tabwriter"
)

// ErrInvalidCLA is the error returned when an invalid command line argument was
// provided, and the usage was already printed.
var ErrInvalidCLA = errors.New("invalid command line args")

// InvalidCLA shows the flag usage, and returns ErrInvalidCLA
func InvalidCLA() error {
	flag.Usage()
	return ErrInvalidCLA
}

// Command is the interface for a command
type Command[Args any] interface {
	// Name returns the name of the command.
	Name() string
	// Desc returns a description of the command.
	Desc() string
	// RegisterFlags registers all the command-specific flags
	// Returns a list of mandatory arguments that must immediately follow the
	// command name
	RegisterFlags(context.Context, Args) ([]string, error)
	// Run invokes the command
	Run(context.Context, Args) error
}

// Run handles the parses the command line arguments, possibly invoking one of
// the provided commands.
// If the command line arguments are invalid, then an error message is printed
// and Run returns ErrInvalidCLA.
func Run[Args any](ctx context.Context, args Args, cmds ...Command[Args]) error {
	_, exe := filepath.Split(os.Args[0])

	flag.Usage = func() {
		out := flag.CommandLine.Output()
		tw := tabwriter.NewWriter(out, 0, 1, 0, ' ', 0)
		fmt.Fprintln(tw, exe, "[command]")
		fmt.Fprintln(tw)
		fmt.Fprintln(tw, "Commands:")
		for _, cmd := range cmds {
			fmt.Fprintln(tw, "  ", cmd.Name(), "\t-", cmd.Desc())
		}
		fmt.Fprintln(tw)
		fmt.Fprintln(tw, "Common flags:")
		tw.Flush()
		flag.PrintDefaults()
	}

	profile := false
	flag.BoolVar(&profile, "profile", false, "enable a webserver at localhost:8080/profile that exposes a CPU profiler")
	mux := http.NewServeMux()
	mux.HandleFunc("/profile", pprof.Profile)

	if len(os.Args) >= 2 {
		help := os.Args[1] == "help"
		if help {
			copy(os.Args[1:], os.Args[2:])
			os.Args = os.Args[:len(os.Args)-1]
		}

		for _, cmd := range cmds {
			if cmd.Name() == os.Args[1] {
				out := flag.CommandLine.Output()
				mandatory, err := cmd.RegisterFlags(ctx, args)
				if err != nil {
					return err
				}
				flag.Usage = func() {
					flagsAndArgs := append(append([]string{}, mandatory...), "<flags>")
					fmt.Fprintln(out, exe, cmd.Name(), strings.Join(flagsAndArgs, " "))
					fmt.Fprintln(out)
					fmt.Fprintln(out, cmd.Desc())
					fmt.Fprintln(out)
					fmt.Fprintln(out, "flags:")
					flag.PrintDefaults()
				}
				if help {
					flag.Usage()
					return nil
				}
				if len(os.Args) < len(mandatory)+2 {
					fmt.Fprintln(out, "missing argument", mandatory[len(os.Args)-2])
					fmt.Fprintln(out)
					return InvalidCLA()
				}
				if err := flag.CommandLine.Parse(os.Args[2:]); err != nil {
					return err
				}
				if profile {
					fmt.Println("download profile at: localhost:8080/profile")
					fmt.Println("then run: 'go tool pprof <file>")
					go http.ListenAndServe(":8080", mux)
				}
				return cmd.Run(ctx, args)
			}
		}
	}

	return InvalidCLA()
}
