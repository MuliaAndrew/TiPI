package logger

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"reflect"
	"strconv"

	"go.uber.org/zap"
)

var (
	LOG_PATH = "/tmp/BDservice.log"
)

type RaftLogEntry struct {
	Op 				string   `json:"operation"`
	Term			uint64   `json:"term"` 
	Ind 			uint64   `json:"index"`
	PrevInd		uint64   `json:"prev_index"`
	PrevTerm	uint64   `json:"prev_term"`
}

type Logger struct {
	logger			zap.SugaredLogger
	log_path		string
	last_entry  RaftLogEntry
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

			op, ok := m["op"].(string)
			if ok {
				l.last_entry.Op = op
			} else {
				return nil, &json.UnmarshalTypeError{
					Value: "op",
					Type: reflect.TypeFor[string](),
				}
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

// append new log massage to the top of log.
// it call logger.Sync() at the end, so user may not call it
func (l* Logger) LogOp(msg RaftLogEntry) {
	l.last_entry = msg
	l.logger.Infow(
		msg.Op,
		"index", msg.Ind,
		"term", msg.Term,
		"prev_index", msg.PrevInd,
		"prev_term", msg.PrevTerm,
	)
	l.Sync()
}

// msgs must be given in the same order as on master
func (l* Logger) LogOps(msgs []RaftLogEntry) {
	for _, m := range msgs {
		l.logger.Infow(
			m.Op,
			"index", m.Ind,
			"term", m.Term,
			"prev_index", m.PrevInd,
			"prev_term", m.PrevTerm,
		)
	}
	l.last_entry = msgs[len(msgs) - 1]
	l.Sync()
}

func (l* Logger) Sync() {
	l.logger.Sync()
}

// Return copy of the last RaftLogEntry currently existing in log
// If no entries was not logged yet, returned default RaftLogEntry
func (l* Logger) LastOp() RaftLogEntry {
	return l.last_entry
}

// Find RaftLogEntry in Raft log and return its position, if didnt found, return last entry
func (l* Logger) FindOp(target RaftLogEntry) (uint64, error) {
	f, err := os.Open(l.log_path)
	if err != nil {
		return 0, err
	}
	defer f.Close()

	f_scanner := bufio.NewScanner(f)
	var nr uint64 = 0
	for f_scanner.Scan() {
		line_str := f_scanner.Text()
		var e RaftLogEntry
		if err := json.Unmarshal([]byte(line_str), &e); err != nil {
			return 0, err
		}
		
		if target == e { return nr, nil }
		
		nr++
	}
	
	return 0, nil
}

// func (l* Logger) EraseUntil()