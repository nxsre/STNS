package main

import (
	"fmt"
	"os"

	"github.com/nxsre/stns/model"
	"github.com/nxsre/stns/server"
	"github.com/nxsre/stns/stns"
	"github.com/urfave/cli/v2"
)

var (
	version   string
	revision  string
	goversion string
	builddate string
	builduser string
)

var flags = []cli.Flag{
	&cli.StringFlag{
		Name:    "logfile",
		Value:   "",
		Usage:   "Log file path",
		EnvVars: []string{"STNS_LOG"},
	},
	&cli.StringFlag{
		Name:    "config",
		Value:   "/etc/stns/server/stns.conf",
		Usage:   "Server config",
		EnvVars: []string{"STNS_CONFIG"},
	},
	&cli.StringFlag{
		Name:    "pidfile",
		Value:   "/var/run/stns.pid",
		Usage:   "pid file path",
		EnvVars: []string{"STNS_PID"},
	},
	&cli.StringFlag{
		Name:    "protocol",
		Value:   "http",
		Usage:   "interface protocol",
		EnvVars: []string{"STNS_PROTOCOL"},
	},
	&cli.StringFlag{
		Name:    "listen",
		Value:   "",
		Usage:   "listern addrand port(xxx.xxx.xxx.xxx:yyy)",
		EnvVars: []string{"STNS_LISTEN"},
	},
}

var commands = []*cli.Command{
	&cli.Command{
		Name:    "server",
		Aliases: []string{"s"},
		Usage:   "Launch core api server",
		Action:  server.LaunchServer,
	},
	&cli.Command{
		Name:    "checkconf",
		Aliases: []string{"c"},
		Usage:   "Check Config",
		Action:  checkConfig,
	},
}

func printVersion(c *cli.Context) {
	fmt.Printf("stns version: %s (%s)\n", version, revision)
	fmt.Printf("build at %s (with %s) by %s\n", builddate, goversion, builduser)
}

func appBefore(c *cli.Context) error {
	// I want to quit this implementation
	if c.String("logfile") != "" {
		os.Setenv("STNS_LOG", c.String("logfile"))
	}

	if c.String("config") != "" {
		os.Setenv("STNS_CONFIG", c.String("config"))
	}

	if c.String("pidfile") != "" {
		os.Setenv("STNS_PID", c.String("pidfile"))
	}

	if c.String("protocol") != "" {
		os.Setenv("STNS_PROTOCOL", c.String("protocol"))
	}

	if c.String("listen") != "" {
		os.Setenv("STNS_LISTEN", c.String("listen"))
	}
	return nil
}

func main() {
	cli.VersionPrinter = printVersion

	app := cli.NewApp()
	app.Name = "stns"
	app.Usage = "Simple Toml Name Service"
	app.Flags = flags

	if len(os.Args) <= 1 {
		app.Action = server.LaunchServer
	} else {
		app.Commands = commands
	}

	app.Before = appBefore

	if err := app.Run(os.Args); err != nil {
		panic(err)
	}
}

func checkConfig(c *cli.Context) error {
	conf, err := stns.NewConfig(os.Getenv("STNS_CONFIG"))
	if err != nil {
		return err
	}
	_, err = model.NewBackendTomlFile(conf.Users, conf.Groups)
	if err == nil {
		fmt.Println("config is good!!1")
	}
	return err
}
