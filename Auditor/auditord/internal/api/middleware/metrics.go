package middleware

import (
	"net/http"
	"strconv"
	"time"

	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/metrics"
)

type metricsResponseWriter struct {
	http.ResponseWriter
	status int
}

func (w *metricsResponseWriter) WriteHeader(status int) {
	w.status = status
	w.ResponseWriter.WriteHeader(status)
}

func Metrics(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()

		mrw := &metricsResponseWriter{
			ResponseWriter: w,
			status:         http.StatusOK,
		}

		next.ServeHTTP(mrw, r)

		duration := time.Since(start).Seconds()
		status := strconv.Itoa(mrw.status)

		metrics.HttpRequestsTotal.WithLabelValues(r.Method, r.URL.Path, status).Inc()
		metrics.HttpRequestDuration.WithLabelValues(r.Method, r.URL.Path, status).Observe(duration)

		metrics.IncrementRequests()
		metrics.IncrementStatusCode(mrw.status)
	})
}