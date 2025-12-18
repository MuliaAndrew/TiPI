package metrics

import (
	"sync"
	"sync/atomic"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
)

var (
	startTime     time.Time
	startOnce     sync.Once
	version       string
	requestsTotal atomic.Int64
	statusCodes   sync.Map
)

var (
	HttpRequestsTotal = promauto.NewCounterVec(
		prometheus.CounterOpts{
			Name: "audit_http_requests_total",
			Help: "Total number of HTTP requests",
		},
		[]string{"method", "path", "status"},
	)

	HttpRequestDuration = promauto.NewHistogramVec(
		prometheus.HistogramOpts{
			Name:    "audit_http_request_duration_seconds",
			Help:    "HTTP request duration in seconds",
			Buckets: prometheus.DefBuckets,
		},
		[]string{"method", "path", "status"},
	)

	EventsCreated = promauto.NewCounter(
		prometheus.CounterOpts{
			Name: "audit_events_created_total",
			Help: "Total number of audit events created",
		},
	)

	EventsQueried = promauto.NewCounter(
		prometheus.CounterOpts{
			Name: "audit_events_queried_total",
			Help: "Total number of audit event queries",
		},
	)

	DbConnectionsActive = promauto.NewGauge(
		prometheus.GaugeOpts{
			Name: "audit_db_connections_active",
			Help: "Number of active database connections",
		},
	)

	DbQueryDuration = promauto.NewHistogramVec(
		prometheus.HistogramOpts{
			Name:    "audit_db_query_duration_seconds",
			Help:    "Database query duration in seconds",
			Buckets: prometheus.DefBuckets,
		},
		[]string{"operation"},
	)
)

func Init(ver string) {
	startOnce.Do(func() {
		startTime = time.Now()
		version = ver
	})
}

func GetStartTime() time.Time {
	return startTime
}

func GetVersion() string {
	return version
}

func IncrementRequests() {
	requestsTotal.Add(1)
}

func GetTotalRequests() int64 {
	return requestsTotal.Load()
}

func IncrementStatusCode(code int) {
	val, _ := statusCodes.LoadOrStore(code, new(atomic.Int64))
	val.(*atomic.Int64).Add(1)
}

func GetStatusCodes() map[int]int64 {
	result := make(map[int]int64)
	statusCodes.Range(func(key, value interface{}) bool {
		result[key.(int)] = value.(*atomic.Int64).Load()
		return true
	})
	return result
}

type Stats struct {
	Version       string        `json:"version"`
	Uptime        string        `json:"uptime"`
	UptimeSeconds float64       `json:"uptime_seconds"`
	StartTime     string        `json:"start_time"`
	TotalRequests int64         `json:"total_requests"`
	StatusCodes   map[int]int64 `json:"status_codes"`
}

func GetStats() Stats {
	uptime := time.Since(startTime)
	return Stats{
		Version:       version,
		Uptime:        uptime.Round(time.Second).String(),
		UptimeSeconds: uptime.Seconds(),
		StartTime:     startTime.Format(time.RFC3339),
		TotalRequests: GetTotalRequests(),
		StatusCodes:   GetStatusCodes(),
	}
}