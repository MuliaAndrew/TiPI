package main

import (
	"context"
	"flag"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/api"
	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/config"
	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/metrics"
	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/storage"
	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/service"
)

var Version = "dev"

func main() {
	env := flag.String("env", "dev", "Environment (dev, test, prod)")
	configPath := flag.String("config", "", "Path to config file (overrides -env)")
	flag.Parse()

	metrics.Init(Version)

	var cfgPath string
	if *configPath != "" {
		cfgPath = *configPath
	} else {
		cfgPath = "config/" + *env + ".yaml"
	}

	cfg := config.MustLoad(cfgPath)

	var logger *slog.Logger
	if cfg.Logging.Format == "json" {
		logger = slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{
			Level: parseLogLevel(cfg.Logging.Level),
		}))
	} else {
		logger = slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{
			Level: parseLogLevel(cfg.Logging.Level),
		}))
	}

	logger.Info("starting audit service",
		"version", Version,
		"env", *env,
		"config", cfgPath,
		"host", cfg.Server.Host,
		"port", cfg.Server.Port,
	)

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	dbCtx, dbCancel := context.WithTimeout(ctx, 10*time.Second)
	defer dbCancel()

	pool, err := storage.NewPool(dbCtx, cfg.Database)
	if err != nil {
		logger.Error("failed to connect to database", "error", err)
		os.Exit(1)
	}
	defer pool.Close()

	logger.Info("connected to database",
		"host", cfg.Database.Host,
		"port", cfg.Database.Port,
		"name", cfg.Database.Name,
	)

	eventRepo := storage.WrapPool(pool, config.RepositoryConfig{})
	eventService := service.NewEventService(eventRepo)

	router := api.NewRouter(eventService, logger)

	server := &http.Server{
		Addr:         cfg.Server.Address(),
		Handler:      router,
		ReadTimeout:  cfg.Server.ReadTimeout,
		WriteTimeout: cfg.Server.WriteTimeout,
	}

	go func() {
		logger.Info("server listening", "address", cfg.Server.Address())
		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			logger.Error("server error", "error", err)
			os.Exit(1)
		}
	}()

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	logger.Info("shutting down server...")

	cancel() // Stop Kafka consumer

	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer shutdownCancel()

	if err := server.Shutdown(shutdownCtx); err != nil {
		logger.Error("server shutdown error", "error", err)
	}
	
	logger.Info("server stopped")
}

func parseLogLevel(level string) slog.Level {
	switch level {
	case "debug":
		return slog.LevelDebug
	case "info":
		return slog.LevelInfo
	case "warn":
		return slog.LevelWarn
	case "error":
		return slog.LevelError
	default:
		return slog.LevelInfo
	}
}