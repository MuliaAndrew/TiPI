package raft

import (
	"bytes"
	"encoding/json"
	"fmt"
	"math/rand/v2"
	"net"
	"net/http"
	"strings"
	"sync/atomic"
	"time"

	// "DB/internal/kvstorage"
	"DB/internal/logger"
	// "github.com/recoilme/pudge"
)

type Mean string

const(
	MEAN_HEARTBEAT Mean = "heartbeat"
	MEAN_LOGAPPEND Mean = "logappend"
	MEAN_LOGCOMMIT Mean = "logcommit"
)

/*
	Send Heartbeat message to peer
	endpoint -- /heartbeat

	Used in 3 cases:
	1. Send empty on timeout=HEARTBEAT_FREQ
	2. Send with replicating log
	3. Send with commit replicated log
*/ 
func SendReplicate(mean Mean, peer net.IP, term uint64, raftlog logger.RaftLogEntry, resp_ch chan<- Rsp)  {
	// send to closed channel
	defer func() { recover() }()

	// heartbeat request body
	body := struct {
		Term     uint64               `json:"term"`
		RaftLog	 logger.RaftLogEntry	`json:"log_entry"`
	}{
		Term: term,
		RaftLog: raftlog,
	}
	body_json, _ := json.Marshal(body)
	body_reader := bytes.NewReader(body_json)

	// sending heartbeats
	url := fmt.Sprintf("http://%s:40404/replicate/%s", peer, mean)
	resp, err := http.Post(url, "application/json", body_reader)


	if err != nil || resp == nil || resp.StatusCode != 200 {
		resp_ch <- Rsp{err: http.ErrServerClosed} // unify error because we anyway process all of them as peer fatal
	}

	defer resp.Body.Close()

	// parse body
	decoder := json.NewDecoder(resp.Body)
	var resp_body struct {
		Len   uint64  `json:"len"`       // peers RaftLog.Uncommited()
		Ack     bool  `json:"commited"`  // false means RaftLog.Uncommited() > RaftLog.Commited(), otherwise true
	}

	if err := decoder.Decode(&resp_body); err != nil {
		resp_ch <- Rsp{err: http.ErrServerClosed} // unify error because we anyway process all of them as peer fatal
	}

	// ok peer response
	resp_ch <- Rsp{
		ip: peer, 
		len: resp_body.Len,
		ans: resp_body.Ack,
	}
}


func throttling() time.Duration {
	return time.Duration(rand.Int() % 1000 * int(HEARTBEAT_TOUT) / 3000) // 1/3 * rand([0,1]) * HEARTBEAT_TOUT
}

// On receiving Heartbeat, LogAppend, LogCommit
// for SendReplicate(Heartbeat, ...) on /heartbeat
//     SendReplicate(LogAppend, ...) on /logappend
//     SendReplicate(LogCommit, ...) on /logcommit
func HandleReplicate(w http.ResponseWriter, req *http.Request, mw *MiddleWare) {
	mean := Mean(req.PathValue("mean"))
	switch req.Method {
	case http.MethodPost:

		// Decode req_body
		var req_body struct {
			Term     uint64               `json:"term"`
			RaftLog	 logger.RaftLogEntry	`json:"log_entry"`
		}
		dec := json.NewDecoder(req.Body)
		if err := dec.Decode(&req_body); err != nil {
			w.WriteHeader(400)
			fmt.Fprintf(w, "Json decoder setup error: %s", err.Error())
			return
		}
		logLen := req_body.RaftLog.Ind
		if req_body.RaftLog.Op != "" {
			logLen++
		}

		mw.Cond.L.Lock()
		defer mw.Cond.L.Unlock()

		switch atomic.LoadUint32(&mw.State) {
		
		//--------- receive heartbeat as condidate or master ----------
		case MASTER:
			if atomic.LoadUint64(&mw.Term) >= req_body.Term {
				// condidate in term where we are master, ignore any incoming replication request
				w.WriteHeader(400)
				break
			}
			// mw.Term < leader.Term
			fallthrough
		case CONDIDATE:
			// need to check check term, if greater than ours, become slave and respond with current log index
			// so the master would start replication of missing logs and commits to us

			if atomic.LoadUint64(&mw.Term) <= req_body.Term {
				// become slave and send our last log
				atomic.StoreUint64(&mw.Term, req_body.Term)
				atomic.StoreUint32(&mw.State, SLAVE)
				mw.LeaderIP = net.ParseIP(reqIP(req))
				mw.Co_done_ctx() // stop _co() functions
				mw.Log.Printf("heartbeat from (%s) with higher term (%d)", mw.LeaderIP, req_body.Term)

				resp_body := struct {
					Len   uint64  `json:"len"`
					Ack     bool  `json:"commited"`
				}{
					Len: mw.RaftLog.LenUncommited(),
					Ack: mw.RaftLog.LenUncommited() == mw.RaftLog.LenCommited(),
				}
				resp_body_json, _ := json.Marshal(resp_body)
				w.WriteHeader(200)
				w.Write(resp_body_json)

				// reset timer
				mw.Timer.Reset(HEARTBEAT_TOUT + throttling())

			} else {
				w.WriteHeader(400)
				fmt.Fprintf(w, "")
			}
			// otherwise do not answer

		//---------------- receive heartbeat as slave -----------------
		case SLAVE:

			// if our Term is less or equal to masters, just answer with log index number
			// if received term less then ours, that meens the sender is older master,
			// which was disconected from replication cluster for long time, os we ignore him
			// because newer master will send to him heartbeat with higher
			if atomic.LoadUint64(&mw.Term) < req_body.Term {
				atomic.StoreUint64(&mw.Term, req_body.Term)
				prefix := fmt.Sprintf("[%s | slave | term: %d] ", mw.SelfIp, mw.Term)
				mw.Log.SetPrefix(prefix)
			} 
			if atomic.LoadUint64(&mw.Term) > req_body.Term {
				// something strange, probably very old master returned 
				w.WriteHeader(400)

				break
			} 
			
			switch mean {
			case MEAN_LOGAPPEND, MEAN_LOGCOMMIT:
				if req_body.RaftLog.Ind == 0 && mw.RaftLog.LenUncommited() == 0 || req_body.RaftLog.PrevInd == mw.RaftLog.LenCommited() - 1 {
					if mw.RaftLog.LenUncommited() > mw.RaftLog.LenCommited() {
						mw.RaftLog.FlushUncommited()
					}
					mw.Log.Printf("logentry = {.term = %d, .log_len = %d}", req_body.Term, logLen)
					mw.RaftLog.LogOp(req_body.RaftLog)
				}
				if mean == MEAN_LOGCOMMIT {
					if mw.RaftLog.LenUncommited() - 1 != req_body.RaftLog.Ind {
						mw.Log.Fatalf("LenUncommited-1 (%d) != req_body.RaftLog.Ind (%d)", mw.RaftLog.LenUncommited() - 1, req_body.RaftLog.Ind)
					}
					mw.Log.Printf("logcommit = {.term = %d, .log_len = %d}", req_body.Term, logLen)
					mw.RaftLog.LogCommit()
					last := mw.RaftLog.LastOp()
					applyToKvs(mw, last)
				}
				fallthrough
			case MEAN_HEARTBEAT:
				resp_body := struct {
					Len   uint64  `json:"len"`
					Ack     bool  `json:"commited"`
				}{
					Len: mw.RaftLog.LenUncommited(),
					Ack: mw.RaftLog.LenUncommited() == mw.RaftLog.LenCommited(),
				}
				resp_body_json, _ := json.Marshal(resp_body)
				w.WriteHeader(200)
				w.Write(resp_body_json)

				if mean == MEAN_HEARTBEAT {
					mw.Log.Printf("heartbeat = {.term = %d, .log_len = %d}", req_body.Term, logLen)
				}

				// reset timer as we successfuly got heartbeat ot logappend or logcommit
				mw.Timer.Reset(HEARTBEAT_TOUT + throttling())
			default:
				w.WriteHeader(400)
				fmt.Fprintf(w, "unsupported Mean (%s)", string(mean))
				mw.Log.Printf("unsupported Mean (%s)", string(mean))
			}

		default:
			mw.Log.Fatalf("mw.State is undefined, terminating")
		}

	default:
		w.WriteHeader(400)
		fmt.Fprintf(w, `
		  Must be a POST request with JSON body
		`)
	}
}


/*
	request: json
	POST
	{
		"log_index":     uint64,
		"log_term":      uint64,
		"election_term": uint64
	}

	response: json
	200 OK
	{
		"term":   uint64,
		"answer": bool
	}
*/

func SendVoteRequest_co(peer net.IP, logLen uint64, logTerm uint64, elTerm uint64, resp_ch chan<-Rsp) {
	req_body := struct {
		LogLen      uint64	`json:"log_len"` 
		LogTerm     uint64	`json:"log_term"`
		ElTerm      uint64  `json:"election_term"`
	} {
		LogLen:   logLen,
		LogTerm:  logTerm,
		ElTerm:   elTerm,
	}
	req_body_json, _ := json.Marshal(&req_body)
	req := bytes.NewReader(req_body_json)

	url := fmt.Sprintf("http://%s:40404/vote", peer)
	resp, err := http.Post(url, "application/json", req)

	if err != nil || resp.StatusCode != 200 {
		resp_ch <- Rsp{err: http.ErrServerClosed, ip: peer} // unify error because we anyway process all of them as fatal
		return
	}

	// parse body
	var resp_body struct {
		Term      uint64	`json:"term"`
		Answer    bool    `json:"answer"` 
	}
	decoder := json.NewDecoder(resp.Body)
	decoder.Decode(&resp_body)

	resp_ch <- Rsp{
		ip:   peer,
		term: resp_body.Term,
		ans:  resp_body.Answer,
	}

	recover()
}

// Vote for first SendVoteRequest() received one
// endpoint -- /vote
// method POST
func HandleVoteRequest(w http.ResponseWriter, req *http.Request, mw *MiddleWare) {
	switch req.Method {
	case http.MethodPost:
		var c struct { // c for condidate
			LogLen      uint64	`json:"log_len"` 
			LogTerm     uint64	`json:"log_term"`
			ElTerm      uint64  `json:"election_term"`
		} 
		dec := json.NewDecoder(req.Body)
		dec.Decode(&c)
		
		// check correctness of received ip address 
		c_ip := reqIP(req)
		ok_ip := false
		for _, peer := range mw.OtherIp {
			if peer.String() == c_ip {
				ok_ip = true
				break
			}
		}
		if ok_ip == false {
			w.WriteHeader(405)
			fmt.Fprintf(w, "This endpoint only for peers, your ip (%s)", c_ip)
		}

		mw.Cond.L.Lock()
		mw.Log.Printf("Vote request from (%s) with election term (%d) and log length (%d)", c_ip, c.ElTerm, c.LogLen)
		Term := atomic.LoadUint64(&mw.Term)
		logTerm := mw.RaftLog.LastOp().Term
		logLen := mw.RaftLog.LenCommited()
		
		logOk := (c.LogTerm > logTerm) || (c.LogTerm == logTerm && c.LogLen >= logLen)
	
		termOk := (c.ElTerm > Term) || (c.ElTerm == Term && mw.VotedFor != nil && mw.VotedFor.String() == c_ip)
	
		if termOk && logOk {
			atomic.StoreUint64(&mw.Term, c.ElTerm)
			atomic.StoreUint32(&mw.State, SLAVE)
			prefix := fmt.Sprintf("[%s | slave | term: %d] ", mw.SelfIp, mw.Term)
			mw.Log.SetPrefix(prefix)
			mw.VotedFor = net.ParseIP(c_ip)
			mw.LeaderIP = net.ParseIP(c_ip)
			mw.Co_done_ctx()
			mw.Timer.Reset(HEARTBEAT_TOUT + throttling())
			mw.Log.Printf("Vote for peer (%s) with election term (%d)", c_ip, c.ElTerm)
		} else {
			mw.Log.Printf("Did not vote for peer (%s) with election term (%d)", c_ip, c.ElTerm)
		}
		mw.Cond.L.Unlock()
	
		w.WriteHeader(200)
		resp_body := struct {
			Term      uint64	`json:"term"`
			Answer    bool    `json:"answer"` 
		} {
			Term: atomic.LoadUint64(&mw.Term),
			Answer: termOk && logOk,
		}
		resp_body_json, _ := json.Marshal(resp_body)
		w.Write(resp_body_json)
	default:
		w.WriteHeader(400)
		fmt.Fprintf(w, "Method (%s) is unsupported", req.Method)
	}
}

func reqIP(req *http.Request) string {
	IPAddress := req.Header.Get("X-Real-Ip")
	if IPAddress == "" {
		IPAddress = req.Header.Get("X-Forwarded-For")
	}
	if IPAddress == "" {
		IPAddress = req.RemoteAddr
	}
	return strings.Split(IPAddress, ":")[0]
}

func applyToKvs(mw *MiddleWare, raftlog logger.RaftLogEntry) {
	switch raftlog.Op {
	case logger.OP_CREATE, logger.OP_UPDATE:
		mw.Kvs.Write(raftlog.Operand, raftlog.Value1)
	case logger.OP_READ:
		// no changes
	case logger.OP_DELETE:
		mw.Kvs.Delete(raftlog.Operand)
	case logger.OP_CAS:
		// TODO
	}
}