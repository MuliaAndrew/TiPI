package main

import (
	"DB/internal/logger"
	"bufio"
	"flag"
	"fmt"
	"net"
	"os"
)

var(
	l 					*log.Logger
	cfg_path		string 			// path to file with array of services ip addresses
	log_path 		string			// path to the log file
	self_ip_str	string			// address of self

	self_ip 		net.IP
	other_ip		[]net.IP
)

func init() {
	flag.StringVar(&self_ip_str, "addr", "", "`addr` to self in cluster network")
	flag.StringVar(&cfg_path, "config", "", "`path` to config file")
	flag.StringVar(&log_path, "log", "", "`path` to Raft log file")
	other_ip = make([]net.IP, 0)
}

func parseArgsAndEnv() {
	flag.Parse()
	self_ip = net.ParseIP(self_ip_str)
}

func parseServiceConfig() {
	f, err := os.Open(cfg_path)
	if err != nil {
		fmt.Fprintf(os.Stderr, "error while parsing config file %s: %s", cfg_path, err.Error())
		os.Exit(1)
	}
	defer f.Close()

	f_scanner := bufio.NewScanner(f)

	for f_scanner.Scan() {
		addr_str := f_scanner.Text()
		other_ip = append(other_ip, net.ParseIP(addr_str))
	}
}

func main() {
	parseArgsAndEnv()

	if log_path == "" {
		log_path = log.LOG_PATH
	}

	if cfg_path == "" {
		flag.PrintDefaults()
		os.Exit(1)
	}

	parseServiceConfig()

	fmt.Printf("Started services at %s with other services:\n", self_ip_str)
	for _, node_addr := range other_ip {
		fmt.Println(node_addr.String())
	}

	l, err := log.InitOrRestore(log_path)
	if err != nil {
		fmt.Printf("logger init: %s\n", err.Error())
		os.Exit(1)
	}
	defer l.Sync()

	l.LogOp(log.RaftLogEntry{
		Op: 			"add x 2",
		Term: 		0,
		Ind: 			0,
		PrevInd: 	0,
		PrevTerm: 0,
	})

}