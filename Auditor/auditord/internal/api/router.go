package api

import (
	"log/slog"
	"net/http"

	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/api/handlers"
	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/api/middleware"
	"github.com/go-chi/chi/v5"
	"github.com/prometheus/client_golang/prometheus/promhttp"
)

func handleHealth(w http.ResponseWriter, req* http.Request) {
	w.WriteHeader(200)
}

func NewRouter(eventService handlers.EventService, logger *slog.Logger) *chi.Mux {
	router := chi.NewRouter()
	
	eventHandler := handlers.NewEventHandler(eventService, logger)

	router.Use(middleware.Recoverer(logger))
	router.Use(middleware.Metrics)
	router.Use(middleware.Logging(logger))

	router.Post("/events", eventHandler.PostEvent)
	router.Get("/events", eventHandler.GetEvents)

	router.Options("/health", handleHealth)

	router.Handle("/metrics", promhttp.Handler())

	return router
}