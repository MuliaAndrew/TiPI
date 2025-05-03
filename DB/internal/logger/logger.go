package logger

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"reflect"
	"slices"
	"strconv"
	"sync"

	"go.uber.org/zap"
)

var (
	LOG_PATH = "/tmp/BDservice.log"
)

type Opertn = string 
const (
	OP_CREATE Opertn = "create"
	OP_UPDATE Opertn = "update"
	OP_READ   Opertn = "read"
	OP_DELETE Opertn = "delete"
	OP_CAS    Opertn = "CAS"
)

type RaftLogEntry struct {
	Op 				Opertn   `json:"operation"`
	Operand   string   `json:"operand"`
	Value1    string   `json:"value1"`
	Value2    string   `json:"value2"` // for CAS
	Term			uint64   `json:"term"` 
	Ind 			uint64   `json:"index"`
	PrevInd		uint64   `json:"prev_index"`
	PrevTerm	uint64   `json:"prev_term"`
}

type Logger struct {
	m           *sync.Mutex
	logger			zap.SugaredLogger
	log_path		string
	last_entry  RaftLogEntry
	to_commit   []RaftLogEntry
}

func checkFileExists(filePath string) bool {
	_, error := os.Stat(filePath)
	//return !os.IsNotExist(err)
	return !errors.Is(error, os.ErrNotExist)
}

// If log file already exists, open it in append, rw mode 
func InitOrRestore(path string) (*Logger, error) {
	var l = new(Logger)
	l.log_path = path
	l.m = &sync.Mutex{}
	l.to_commit = make([]RaftLogEntry, 0, 100) // buffer for 100 uncommited changes

	if checkFileExists(path) {
		f, err := os.Open(path)
		if err != nil {
			fmt.Fprintf(os.Stderr, err.Error())
			os.Exit(1)
		}
		defer f.Close()

		f_scanner := bufio.NewScanner(f)
		if f_scanner.Scan() == false {
			l.last_entry.Ind = 0
			l.last_entry.Op = ""
			l.last_entry.Term = 0
		} else {
			top_log_str := f_scanner.Text()
			top_log := []byte(top_log_str)

			m := make(map[string]any)
			if err := json.Unmarshal(top_log, &m); err != nil {
				return nil, err
			}

			term, ok := m["term"].(string)
			if ok == true {
				l.last_entry.Term, err = strconv.ParseUint(term, 10, 64)
			} else {
				return nil, &json.UnmarshalTypeError{
					Value: "term",
					Type: reflect.TypeFor[uint64](),
				}
			}

			ind, ok := m["index"].(string)
			if ok == true {
				l.last_entry.Ind, err = strconv.ParseUint(ind, 10, 64)
			} else {
				return nil, &json.UnmarshalTypeError{
					Value: "index",
					Type: reflect.TypeFor[uint64](),
				}
			}

			prev_term, ok := m["prev_term"].(string)
			if ok == true {
				l.last_entry.PrevTerm, err = strconv.ParseUint(prev_term, 10, 64)
			} else {
				return nil, &json.UnmarshalTypeError{
					Value: "prev_term",
					Type: reflect.TypeFor[uint64](),
				}
			}

			prev_ind, ok := m["prev_index"].(string)
			if ok == true {
				l.last_entry.PrevInd, err = strconv.ParseUint(prev_ind, 10, 64)
			} else {
				return nil, &json.UnmarshalTypeError{
					Value: "prev_index",
					Type: reflect.TypeFor[uint64](),
				}
			}
		}
	} else {
		f, err := os.Create(path)
		if err != nil {
			return nil, err
		}
		f.Close()
	}

	cfg_json_format := `{
		"level": "info",
		"encoding": "json",
		"outputPaths": ["%s"],
		"errorOutputPaths": ["stderr"],
		"encoderConfig": {
			"messageKey": "op"
		}
	}` // op is Create Read Update Delete

	cfg_json_str := fmt.Sprintf(cfg_json_format, path)
	cfg_json := []byte(cfg_json_str)

	var cfg zap.Config
	if err := json.Unmarshal(cfg_json, &cfg); err != nil {
		return nil, err
	}

	l.logger = *zap.Must(cfg.Build()).Sugar()

	return l, nil
}

// append new uncommited log massage to the top uncommited log lost.
// Op Operand Value1 [Value2] Term must be specified, the rest
// will be setted automatically
func (l* Logger) LogOp(msg RaftLogEntry) {
	l.m.Lock()
	defer l.m.Unlock()
	if l.to_commit != nil && len(l.to_commit) > 0 {
		msg.Ind = l.to_commit[len(l.to_commit) - 1].Ind + 1
		msg.PrevInd = msg.Ind - 1
		msg.PrevTerm = l.to_commit[len(l.to_commit) - 1].Term
	} else {
		msg.Ind = l.last_entry.Ind + 1
		msg.PrevInd = msg.Ind - 1
		msg.PrevTerm = l.last_entry.Term
	}
	l.to_commit = append(l.to_commit, msg)
}

// msgs must be given in the same order as on master
func (l* Logger) LogOps(msgs []RaftLogEntry) {
	l.m.Lock()
	defer l.m.Unlock()
	if msgs == nil {
		return
	}
	if l.to_commit != nil && len(l.to_commit) > 0 {
		msgs[0].Ind = l.to_commit[len(l.to_commit) - 1].Ind + 1
		msgs[0].PrevInd = msgs[0].Ind - 1
		msgs[0].PrevTerm = l.to_commit[len(l.to_commit) - 1].Term
	} else {
		msgs[0].Ind = l.last_entry.Ind + 1
		msgs[0].PrevInd = msgs[0].Ind - 1
		msgs[0].PrevTerm = l.last_entry.Term
	}
	for i := range msgs {
		if i == 0 { continue }
		msgs[i].Ind = l.to_commit[len(l.to_commit) - 1].Ind + 1
		msgs[i].PrevInd = msgs[i].Ind - 1
		msgs[i].PrevTerm = l.to_commit[len(l.to_commit) - 1].Term
	}
	l.to_commit = append(l.to_commit, msgs...)
}

// apply uncommited log list to logger
func (l* Logger) LogCommit() {
	l.m.Lock()
	defer l.m.Unlock()
	for _, m := range l.to_commit {
		l.logger.Infow(
			m.Op,
			"operand", m.Operand,
			"value1", m.Value1,
			"value2", m.Value2,
			"index", m.Ind,
			"term", m.Term,
			"prev_index", m.PrevInd,
			"prev_term", m.PrevTerm,
		)
	}
	l.last_entry = l.to_commit[len(l.to_commit) - 1]
	l.to_commit = make([]RaftLogEntry, 0, 100) // flush buffer
}

// Return copy of the last RaftLogEntry currently existing in log
// If no entries was not logged yet, returned default RaftLogEntry
func (l* Logger) LastOp() RaftLogEntry {
	l.m.Lock()
	defer l.m.Unlock()
	last := l.last_entry
	return last
}

func (l *Logger) LenCommited() uint64 {
	l.m.Lock()
	defer l.m.Unlock()
	lenght := l.last_entry.Ind
	if l.last_entry.Op != "" { 
		lenght += 1
	}
	return lenght
}

// LenCommited() + len(to_commit)
func (l* Logger) LenUncommited() uint64 {
	l.m.Lock()
	defer l.m.Unlock()
	lenght := l.last_entry.Ind
	if l.last_entry.Op != "" { 
		lenght += 1
	}
	if l.to_commit != nil {
		lenght += uint64(len(l.to_commit))
	}
	return lenght
}

// Find RaftLogEntry in Raft log and return its position, if didnt found, return last entry
func (l* Logger) FindOp(target RaftLogEntry) (uint64, error) {
	l.m.Lock()
	defer l.m.Unlock()
	f, err := os.Open(l.log_path)
	if err != nil {
		return 0, err
	}
	defer f.Close()

	f_scanner := bufio.NewScanner(f)
	var nr uint64 = 0
	for f_scanner.Scan() {
		line_str := f_scanner.Text()
		var entry RaftLogEntry
		if err := json.Unmarshal([]byte(line_str), &entry); err != nil {
			return 0, err
		}
		
		if target == entry { return nr, nil }
		
		nr++
	}
	
	return 0, nil
}

// return array of commited RaftLogEntry log[index:len(log)-1], if index>=len(log)-1 return nil
// expensive operation
func (l* Logger) Ops(index uint64) []RaftLogEntry {
	l.m.Lock()
	defer l.m.Unlock()
	
	if index >= l.last_entry.Ind {
		return nil
	}
	
	f, err := os.Open(l.log_path)
	if err != nil {
		return nil
	}
	defer f.Close()

	res := make([]RaftLogEntry, 0)
	f_scanner := bufio.NewScanner(f)
	for f_scanner.Scan() {
		line_str := f_scanner.Text()
		var entry RaftLogEntry
		if err := json.Unmarshal([]byte(line_str), &entry); err != nil {
			return nil
		}

		if index >= entry.Ind {
			res = append(res, entry)
		}
	}
	slices.Reverse(res)
	return res
}

func (l* Logger) UncommitedOps() []RaftLogEntry {
	l.m.Lock()
	defer l.m.Unlock()
	res := make([]RaftLogEntry, len(l.to_commit))
	copy(res, l.to_commit)
	return res
}