package models

import (
	"encoding/json"
	"time"
)

type Event struct {
	Ts          string           `json:"timestamp"`
	User        string           `json:"user"`
	Component   string           `json:"component,omitempty"`
	Op          string           `json:"op"`
	Session_id  int64            `json:"session_id,omitempty"`
	Req_id      int64            `json:"req_id,omitempty"`
	Res         json.RawMessage  `json:"res,omitempty"`
	Attrs       json.RawMessage  `json:"attributes,omitempty"`
}

type PostEventRequest Event

type PostEventResponse struct {}

type GetEventsQueryRequest struct {
	Ts_start   time.Time
	Ts_end     time.Time
	Ts         time.Time
	Component  []string
	User       []string
	Op         []string
	Session_id []int64
	Req_id     []int64
  // Custom_filter []Filter
}

type GetEventsQueryResponse struct {
	Events     []*Event           `json:"events"`
}

type ErrorResponse struct {
	Status  string `json:"status"`
	Message string `json:"message"`
	Details string `json:"details"`
}