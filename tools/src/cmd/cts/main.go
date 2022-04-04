package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
)

var errInvalidCLA = errors.New("invalid command line args")

func main() {
	ctx := context.Background()
	if err := run(ctx); err != nil {
		if err != errInvalidCLA {
			fmt.Fprintln(os.Stderr, err)
		}
		os.Exit(1)
	}
}

func invalidCLA() error {
	flag.Usage()
	return errInvalidCLA
}

type Command interface {
	name() string
	desc() string
	registerFlags(ctx context.Context, cfg Config) ([]string, error)
	run(ctx context.Context, cfg Config) error
}

func run(ctx context.Context) error {
	cfg, err := loadConfig(filepath.Join(thisDir(), "config.json"))
	if err != nil {
		return err
	}

	cmds := []Command{
		&cmdFormat{},
		&cmdMerge{},
		&cmdResults{},
		&cmdRoll{},
		&cmdUpdate{},
	}

	flag.Usage = func() {
		out := flag.CommandLine.Output()
		fmt.Fprintln(out, "cts [command]")
		fmt.Fprintln(out)
		fmt.Fprintln(out, "Commands:")
		for _, cmd := range cmds {
			fmt.Fprintln(out, "  ", cmd.name())
		}
		flag.PrintDefaults()
	}

	if len(os.Args) >= 2 {
		help := os.Args[1] == "help"
		if help {
			copy(os.Args[1:], os.Args[2:])
			os.Args = os.Args[:len(os.Args)-1]
		}

		for _, cmd := range cmds {
			if cmd.name() == os.Args[1] {
				out := flag.CommandLine.Output()
				args, err := cmd.registerFlags(ctx, *cfg)
				if err != nil {
					return err
				}
				flag.Usage = func() {
					hasFlags := false
					flag.VisitAll(func(*flag.Flag) { hasFlags = true })
					flagsAndArgs := args
					if hasFlags {
						flagsAndArgs = append(append([]string{}, args...), "<flags>")
					}
					fmt.Fprintln(out, "cts", cmd.name(), strings.Join(flagsAndArgs, " "))
					fmt.Fprintln(out)
					fmt.Fprintln(out, cmd.desc())
					if hasFlags {
						fmt.Fprintln(out)
						fmt.Fprintln(out, "flags:")
						flag.PrintDefaults()
					}
				}
				if help {
					flag.Usage()
					return nil
				}
				if len(os.Args) < len(args)+2 {
					fmt.Fprintln(out, "missing argument", args[len(os.Args)-2])
					fmt.Fprintln(out)
					return invalidCLA()
				}
				if err := flag.CommandLine.Parse(os.Args[2:]); err != nil {
					return err
				}
				return cmd.run(ctx, *cfg)
			}
		}
	}

	return invalidCLA()
}
