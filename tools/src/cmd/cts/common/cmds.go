package common

import (
	"dawn.googlesource.com/dawn/tools/src/subcmd"
)

var commands []Command

// Command is the type of a single cts command
type Command = subcmd.Command[Config]

// Register registers the command for use by the 'cts' tool
func Register(c Command) { commands = append(commands, c) }

// Commands returns all the commands registered
func Commands() []Command { return commands }
