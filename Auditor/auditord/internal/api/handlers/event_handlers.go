package handlers

import (
	"context"
	"encoding/json"
	"log/slog"
	"net/http"
	"net/url"
	"strconv"
	"strings"
	"time"

	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/models"
)

type EventService interface {
	PostEvent(ctx context.Context, req *models.PostEventRequest) (*models.PostEventResponse, error)
	GetEventsQuery(ctx context.Context, req *models.GetEventsQueryRequest) (*models.GetEventsQueryResponse, error)
}

type EventHandler struct {
	service EventService
	logger  *slog.Logger
}

func NewEventHandler(svc EventService, logger *slog.Logger) *EventHandler {
	return &EventHandler{
		service: svc,
		logger:  logger,
	}
}

func (h *EventHandler) PostEvent(w http.ResponseWriter, r *http.Request) {
	var req models.PostEventRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		h.respondError(w, http.StatusBadRequest, "Invalid request body", err.Error())
		return
	}
	if req.Ts == "" {
		h.respondError(w, http.StatusBadRequest, "Invalid request", "Field 'timestamp' is required")
		return
	}
	if req.User == "" {
		h.respondError(w, http.StatusBadRequest, "Invalid request", "Field 'user' is required")
		return
	}
	if req.Op == "" {
		h.respondError(w, http.StatusBadRequest, "Invalid request", "Field 'op' is required")
		return
	}

	resp, err := h.service.PostEvent(r.Context(), &req)
	if err != nil {
		h.logger.Error("failed to create event", "error", err)
		if strings.Contains(err.Error(), "unknown event type") {
			h.respondError(w, http.StatusBadRequest, "Invalid event type", err.Error())
			return
		}
		h.respondError(w, http.StatusInternalServerError, "Internal server error", "")
		return
	}

	h.respondJSON(w, http.StatusCreated, resp)
}

func (h *EventHandler) GetEvents(w http.ResponseWriter, r *http.Request) {
	query := r.URL.Query()

	req, err := parseQuery(&query)
	if err != nil {
		h.respondError(w, http.StatusBadRequest, "Invalid parameter", "Invalid ts format")
		return
	}

	resp, err := h.service.GetEventsQuery(r.Context(), req)

	h.respondJSON(w, http.StatusCreated, resp)
}

func (h *EventHandler) respondJSON(w http.ResponseWriter, status int, data interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	if err := json.NewEncoder(w).Encode(data); err != nil {
		h.logger.Error("failed to encode response", "error", err)
	}
}

func (h *EventHandler) respondError(w http.ResponseWriter, status int, message, details string) {
	resp := models.ErrorResponse{
		Status:  "error",
		Message: message,
		Details: details,
	}
	h.logger.Info("400+ Error", "error", status, "msg", message, "details", details)
	h.respondJSON(w, status, resp)
}

func parseTimestamp(s string) (time.Time, error) {
	formats := []string{
		time.RFC3339,
		time.RFC3339Nano,
		"2006-01-02T15:04:05",
		"2006-01-02T15:04:05.000000",
		"2006-01-02 15:04:05",
	}

	for _, format := range formats {
		if t, err := time.Parse(format, s); err == nil {
			return t, nil
		}
	}

	return time.Time{}, &parseError{msg: "invalid timestamp format: " + s}
}

type parseError struct {
	msg string
}

func (e *parseError) Error() string {
	return e.msg
}

func parseQuery(query *url.Values) (*models.GetEventsQueryRequest, error) {
	req := &models.GetEventsQueryRequest{}
	if v := query.Get("ev_ts_start"); v != "" {
		t, err := parseTimestamp(v)
		if err != nil {
			return req, err
		}
		req.Ts_start = t
	}

	if v := query.Get("ev_ts_end"); v != "" {
		t, err := parseTimestamp(v)
		if err != nil {
			return req, err
		}
		req.Ts_end = t
	}

	if v := query.Get("ev_ts"); v != "" {
		t, err := parseTimestamp(v)
		if err != nil {
			return req, err
		}
		req.Ts = t
	}

	if v := query.Get("ev_op"); v != "" {
		req.Op = strings.Split(v, ",")
	}

	if v := query.Get("ev_user"); v != "" {
		req.User = strings.Split(v, ",")
	}

	if v := query.Get("ev_component"); v != "" {
		req.Component = strings.Split(v, ",")
	}

	if v := query.Get("ev_session_id"); v != "" {
		for _, s := range strings.Split(v, ",") {
			if r, err := strconv.ParseInt(s, 10, 1); err == nil {
				req.Session_id = append(req.Session_id, r)
			} else {
				return req, err
			}
		}
	}

	if v := query.Get("ev_req_id"); v != "" {
		for _, s := range strings.Split(v, ",") {
			if r, err := strconv.ParseInt(s, 10, 1); err == nil {
				req.Req_id = append(req.Req_id, r)
			} else {
				return req, err
			}
		}
	}

	return req, nil
}