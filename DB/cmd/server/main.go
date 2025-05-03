package main

import (
	"bufio"
	"context"
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"sync"
	"sync/atomic"
	"time"

	"DB/internal/kvstorage"
	"DB/internal/logger"
	"DB/internal/raft"
	"DB/internal/rest"
)

var(
	cfg_path		string 			// path to file with array of services ip addresses
	log_path 		string			// path to the log file
	self_ip_str	string			// address of self
	db_path			string			// path to db directory

	mw 					raft.MiddleWare
)

func init() {
	flag.StringVar(&self_ip_str, "addr", "", "`addr` to self in cluster network")
	flag.StringVar(&cfg_path, "config", "", "`path` to config file, must be set")
	flag.StringVar(&log_path, "log", "", "`path` to Raft log file")
	flag.StringVar(&db_path, "db", "", "`path` to the pudge data base directory (app create it if not exist), must be set")
	mw.OtherIp = make([]net.IP, 0)
}

func parseArgsAndEnv() {
	flag.Parse()
	mw.SelfIp = net.ParseIP(self_ip_str)
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
		mw.OtherIp = append(mw.OtherIp, net.ParseIP(addr_str))
	}
}

func main() {

	// Setting up global context
	parseArgsAndEnv()

	if log_path == "" {
		log_path = logger.LOG_PATH
	}

	if cfg_path == "" || db_path == "" {
		flag.PrintDefaults()
		os.Exit(1)
	}

	parseServiceConfig()

	// setting up stdout logger
	prefix := fmt.Sprintf("[%s | none | term: 0] ", mw.SelfIp)
	mw.Log = log.New(os.Stdout, prefix, log.Ldate | log.Ltime | log.Lmicroseconds | log.Lmsgprefix)

	mw.Log.Printf("Started services at %s with other services:\n", self_ip_str)
	for i, node_addr := range mw.OtherIp {
		mw.Log.Printf("node%d addr: %s", i , node_addr.String())
	}

	// Restoring Raft Logs if exist, otherwise create blanc new,
	// starting Raft logger
	l, err := logger.InitOrRestore(log_path)
	if err != nil {
		mw.Log.Fatalf("logger init error: %s\n", err.Error())
	}
	mw.RaftLog = l
	if mw.RaftLog.LastOp().Ind == 0 {
		mw.Log.Printf("Created new raft log")
	} else {
		prefix := fmt.Sprintf("[%s | slave | term: %d] ", mw.SelfIp, mw.RaftLog.LastOp().Term)
		mw.Log.SetPrefix(prefix)
		mw.Log.Printf("Restored raft log with last op = {.Term: %d; .Ind: %d}", mw.RaftLog.LastOp().Term, mw.RaftLog.LastOp().Ind)
	}

	// Open and setting up local database (ondisk key-value storage)
	mw.Kvs = new(kvstorage.Kvs)
	if err := mw.Kvs.Open(db_path); err != nil {
		mw.Log.Fatalf("kvstorage init error: %s", err.Error())
	}

	// Setting up http server middleware and handlers
	middleware := func(f raft.HttpHandler) http.HandlerFunc {
		return func(w http.ResponseWriter, req *http.Request) {
			// // checking where content type is application/json
			// ct := req.Header.Get("Content-Type")
			// if ct == "" {
			// 	msg := "no header Content-Type"
			// 	http.Error(w, msg, http.StatusUnsupportedMediaType)
			// 	return
			// }
			// mediaType := strings.ToLower(strings.TrimSpace(strings.Split(ct, ";")[0]))
			// if mediaType != "application/json" {
			// 	msg := "Content-Type header is not application/json"
			// 	http.Error(w, msg, http.StatusUnsupportedMediaType)
			// 	return
			// }
			
			f(w, req, &mw)
		}
	}

	serverMux := http.NewServeMux()
	// UI for testing
	serverMux.HandleFunc("/", middleware(rest.HandleHello))
	// UI, for POST and PATCH query value="" is neaded 
	serverMux.HandleFunc("/kvs/{id}", middleware(rest.HandleKvs))
	// for SendReplicate_co(MEAN_HEARTBEAT, ...) on /heartbeat
	//     SendReplicate_co(MEAN_LOGAPPEND, ...) on /logappend
  //     SendReplicate_co(MEAN_LOGCOMMIT, ...) on /logcommit
	serverMux.HandleFunc("/replicate/{mean}", middleware(raft.HandleReplicate))
	// fot SendVoteRequest
	serverMux.HandleFunc("/vote", middleware(raft.HandleVoteRequest))

	prefix = fmt.Sprintf("[%s | slave | term: %d] ", mw.SelfIp, mw.RaftLog.LastOp().Term)
	mw.Log.SetPrefix(prefix)
	atomic.StoreUint32(&mw.State, raft.SLAVE)
	mw.LeaderIP = mw.SelfIp
	// first time setting up heartbeat timer
	mw.Timer = time.NewTimer(raft.HEARTBEAT_TOUT)

	// Running server
	srv_err_ch := make(chan error)
	go func() {
		mw.Log.Println("START LISTENNING ON PORT 40404")
		err = http.ListenAndServe(":40404", serverMux)
		
		if err != nil {
			srv_err_ch<- err
			close(srv_err_ch)
		}
	}()

	// Setting up routines for event loop
	bg := context.Background()
	mw.CommitLen = map[string]uint64{}
	mw.SentLen = map[string]uint64{}
	mw.Mu = &sync.Mutex{}
	m := &sync.Mutex{}
	mw.Cond = sync.NewCond(m)
	
	// Setting up event loop and channels for them
	for {
		co_done_ctx, co_done := context.WithCancel(bg)
		mw.Co_done_ctx = co_done
		co_done_ch := make(chan struct{})
		switch atomic.LoadUint32(&mw.State) {
		// 2nd param of _co function -- context which cancel this functions
		// 3rd param -- channel to indicate parent goroutine that this function did return
		case raft.MASTER:
			prefix := fmt.Sprintf("[%s | master | term: %d] ", mw.SelfIp, atomic.LoadUint64(&mw.Term))
			mw.Log.SetPrefix(prefix)
			mw.Log.Println("BECOME MASTER")
			mw.RaftLog.FlushUncommited()

			go raft.Heartbeat_producer_co(&mw, co_done_ctx, co_done_ch)

		case raft.CONDIDATE:
			prefix := fmt.Sprintf("[%s | condidate | term: %d] ", mw.SelfIp, atomic.LoadUint64(&mw.Term))
			mw.Log.SetPrefix(prefix)
			mw.Log.Println("BECOME CONDIDATE")
			mw.RaftLog.FlushUncommited()
			
			go raft.Election_co(&mw, co_done_ctx, co_done_ch)

		case raft.SLAVE:
			prefix := fmt.Sprintf("[%s | slave | term: %d] ", mw.SelfIp, atomic.LoadUint64(&mw.Term))
			mw.Log.SetPrefix(prefix)
			mw.Log.Println("BECOME SLAVE")

			go raft.Heartbeat_consumer_co(&mw, co_done_ctx, co_done_ch)

		}
  
		select {
		case err := <-srv_err_ch:
			co_done()
			// server may exit only on interruption/signal or listener abortion (due to same signals or 
			// port error). in any case, we cannot heal ourself, so we terminating
			mw.Log.Fatalf("server was terminated by error: %s", err.Error())
		case <-co_done_ch:
			// server state had changed
			co_done()
		// do not process <-co_done_ctx.Done() for simple implimentation of _co() functions
		}
	}

}