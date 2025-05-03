package rest

import (
	"encoding/json"
	"errors"
	"fmt"
	"maps"
	"math/rand/v2"
	"net/http"
	"slices"
	"sync/atomic"
	"time"

	"DB/internal/logger"
	"DB/internal/raft"

	"github.com/recoilme/pudge"
)

func HandleHello(w http.ResponseWriter, req *http.Request, mw *raft.MiddleWare) {
	ST_NAME := map[uint32]string{
		raft.MASTER: "master",
		raft.CONDIDATE: "condidate",
		raft.SLAVE: "slave",
	}
	switch req.Method {
	case http.MethodGet:
		w.WriteHeader(200)
		fmt.Fprintf(w, "hello from %s (%s) with term (%d) to %s\n", 
		            mw.SelfIp.String(), 
		            ST_NAME[atomic.LoadUint32(&mw.State)], 
		            atomic.LoadUint64(&mw.Term), 
		            req.RemoteAddr)
	default:
		w.WriteHeader(400)
		fmt.Fprintf(w, "must use GET method on 'hello' resourse\n")
	}
}

/*
create
	POST
	/kvs/{key_name}?value={value}

read
	GET
	/kvs/{key_name}

update
	PATCH	
	/kvs/{key_name}?value={value}

delete
	DELETE
	/kvs/{key_name}
*/

// endpoint -- /kvs/{id} (id == key)
func HandleKvs(w http.ResponseWriter, req *http.Request, mw *raft.MiddleWare) {

	// redirect write requests to the master
	if atomic.LoadUint32(&mw.State) != raft.MASTER && req.Method != http.MethodGet {
		new_url := "http://" + mw.LeaderIP.String() + ":40404" + req.URL.Path + "?" + req.URL.RawQuery
		http.Redirect(w, req, new_url, http.StatusTemporaryRedirect)
		mw.Log.Printf("Redirect %s request to master (%s)", req.Method, mw.LeaderIP.String())
		return
	}

	// key name out of {id}
	key_name := req.PathValue("id")

	switch req.Method {
	//---------------------------------- POST -------------------------------
	case http.MethodPost:
		// Decode query
		value := req.URL.Query().Get("value")
		if value == "" {
			w.WriteHeader(400)
			fmt.Fprintf(w, "Query param value is requiered")
			return
		}

		// Make corresponding raft log and changes
		mw.Cond.L.Lock()

		// if key already exist in kvstorage, its error
		if _, err := mw.Kvs.Read(key_name); err == nil {
			w.WriteHeader(400)
			fmt.Fprintf(w, "Key %s already exist", key_name)
			mw.Cond.L.Unlock()
			return 
		}
		mw.Log.Printf("POST key (%s) value (%s): sent log", key_name, value)

		mw.RaftLog.LogOp(logger.RaftLogEntry{
			Op:        logger.OP_CREATE,
			Operand:   key_name,
			Value1:    value,
			Value2:    "",
			Term:      atomic.LoadUint64(&mw.Term),
		})

		requiredLen := mw.RaftLog.LenUncommited()

		mw.Timer.Reset(time.Microsecond)

		// quorum had been accept this log
		for requiredLen > minSentLenQuorum(mw) {
			mw.Cond.Wait()
		}

		// Store changes to the db
		if err := mw.Kvs.Write(key_name, value); err != nil {
			w.WriteHeader(400)
			fmt.Fprintf(w, "Json decoding error: %s", err.Error())
			mw.Cond.L.Unlock()
			return
		}

		// commit changes
		mw.RaftLog.LogCommit()
		mw.Log.Printf("POST key (%s) value (%s): commit log", key_name, value)

		mw.Cond.L.Unlock()

		// answer to the client
		resp_body := struct {
			Operand   string   `json:"operand"`
			Value     string   `json:"value"`
		}{
			Operand: key_name,
			Value: value,
		}
		resp_body_json, err := json.Marshal(resp_body)
		if err != nil {
			w.WriteHeader(500)
			fmt.Fprintf(w, "response body build error: %s", err.Error())
			return
		}

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(201)

		fmt.Fprint(w, string(resp_body_json))
	
	//---------------------------------- GET -------------------------------
	case http.MethodGet:

		// redirection to slaves
		if atomic.LoadUint32(&mw.State) == raft.MASTER {
			ind := rand.Int() % len(mw.OtherIp)
			if ind != len(mw.OtherIp) {
				// (n-1)/n probability of redirect to slave
				new_url := "http://" + mw.OtherIp[ind].String() + ":40404" + req.URL.Path
				http.Redirect(w, req, new_url, http.StatusTemporaryRedirect)
				mw.Log.Printf("Redirect GET request to slave (%s)", mw.OtherIp[ind].String())
				return
			}
		}

		// Read key from storage
		value, err := mw.Kvs.Read(key_name)
		if err != nil {

			// key not exist in storage
			if errors.Is(err, pudge.ErrKeyNotFound) {
				w.WriteHeader(404)
				mw.Log.Printf("GET key (%s): no such key", key_name)
				fmt.Fprintf(w, "key %s does not exist in data base", key_name)
				return
			} else {
				w.WriteHeader(500)
				fmt.Fprintf(w, "data base error: %s", err.Error())
				return
			}
		}

		mw.Log.Printf("GET key (%s) value (%s)", key_name, value)

		// answer to the client

		resp_body := struct {
			Operand   string	`json:"operand"`
			Value     string	`json:"value"`
		}{
			key_name,
			value,
		}

		resp_body_json, err := json.Marshal(resp_body)
		if err != nil {
			w.WriteHeader(500)
			fmt.Fprintf(w, "response body build error: %s", err.Error())
			return
		}

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(200)

		fmt.Fprint(w, string(resp_body_json))

	//--------------------------------- PATCH ------------------------------
	case http.MethodPatch:
		// Decode query
		value := req.URL.Query().Get("value")
		if value == "" {
			w.WriteHeader(400)
			fmt.Fprintf(w, "Query param value is requiered")
			return
		}

		// Make corresponding raft log and changes
		mw.Cond.L.Lock()

		// Chech if key already present in kvstorage
		_, err := mw.Kvs.Read(key_name)
		if err != nil {
			// key not exist in storage
			if errors.Is(err, pudge.ErrKeyNotFound) {
				w.WriteHeader(404)
				mw.Log.Printf("PATCH key (%s) value (%s): key does not exist log", key_name, value)
				fmt.Fprintf(w, "key %s does not exist in data base", key_name)
			} else {
				w.WriteHeader(500)
				fmt.Fprintf(w, "data base error: %s", err.Error())
			}
			mw.Cond.L.Unlock()
			return
		}

		mw.Log.Printf("PATCH key (%s) value (%s): sent log", key_name, value)

		mw.RaftLog.LogOp(logger.RaftLogEntry{
			Op:        logger.OP_UPDATE,
			Operand:   key_name,
			Value1:    value,
			Value2:    "",
			Term:      atomic.LoadUint64(&mw.Term),
		})

		requiredLen := mw.RaftLog.LenUncommited()

		mw.Timer.Reset(time.Microsecond)

		// quorum had been accept this log
		for requiredLen > minSentLenQuorum(mw) {
			mw.Cond.Wait()
		}

		// Make patch
		if err := mw.Kvs.Write(key_name, value); err != nil {
			w.WriteHeader(500)
			fmt.Fprintf(w, "data base error: %s", err.Error())
			mw.Cond.L.Unlock()
			return
		}
		mw.RaftLog.LogCommit()

		mw.Log.Printf("PATCH key (%s) value (%s): commit log", key_name, value)
		
		mw.Cond.L.Unlock()

		// answer to the client
		resp_body := struct {
			Operand   string	`json:"operand"`
			Value     string	`json:"value"`
		}{
			key_name,
			value,
		}

		resp_body_json, err := json.Marshal(resp_body)
		if err != nil {
			w.WriteHeader(500)
			fmt.Fprintf(w, "response body build error: %s", err.Error())
			return
		}

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(200)

		fmt.Fprint(w, string(resp_body_json))

	//-------------------------------- DELETE ------------------------------
	case http.MethodDelete:
		// Make corresponding raft log and changes
		mw.Cond.L.Lock()

		if _, err := mw.Kvs.Read(key_name); err != nil {
			if errors.Is(err, pudge.ErrKeyNotFound) {
				w.WriteHeader(400)
				fmt.Fprintf(w, "Key %s does not exist in Db", key_name)
				mw.Log.Printf("DELETE key (%s): key does not exist", key_name)
			} else {
				w.WriteHeader(500)
				fmt.Fprintf(w, "data base error: %s", err.Error())
			}
			mw.Cond.L.Unlock()
			return
		}

		mw.Log.Printf("DELETE key (%s): commit log", key_name)

		mw.RaftLog.LogOp(logger.RaftLogEntry{
			Op:        logger.OP_DELETE,
			Operand:   key_name,
			Value1:    "",
			Value2:    "",
			Term:      atomic.LoadUint64(&mw.Term),
		})

		requiredLen := mw.RaftLog.LenUncommited()

		mw.Timer.Reset(time.Microsecond)

		// quorum had been accept this log
		for requiredLen > minSentLenQuorum(mw) {
			mw.Cond.Wait()
		}

		// Make delete
		if err := mw.Kvs.Delete(key_name); err != nil {
			w.WriteHeader(500)
			fmt.Fprintf(w, "data base error: %s", err.Error())
			mw.Cond.L.Unlock()
			return
		}

		mw.RaftLog.LogCommit()
		mw.Log.Printf("DELETE key (%s): commit log", key_name)
		
		mw.Cond.L.Unlock()

		// answer to the client
		resp_body := struct{
			Key 	 string	`json:"operand"`
		}{
			Key: key_name,
		}

		resp_body_json, err := json.Marshal(resp_body)
		if err != nil {
			w.WriteHeader(500)
			fmt.Fprintf(w, "response body build error: %s", err.Error())
			return
		}

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(200)

		fmt.Fprint(w, string(resp_body_json))

	default:
		w.WriteHeader(400)
		fmt.Fprintf(w, "Method (%s) is unsupported", req.Method)
	}
}

// maximum of minimum sentLen among quorums
func minSentLenQuorum(mw *raft.MiddleWare) uint64 {
	lens := slices.Collect(maps.Values(mw.SentLen))
	slices.Sort(lens)
	return lens[len(lens)/2]
}