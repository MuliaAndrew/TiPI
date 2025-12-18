package storage

import (
	"context"
	// "database/sql"
	"errors"
	"fmt"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/config"
	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/models"
)

func NewPool(ctx context.Context, cfg config.DatabaseConfig) (*pgxpool.Pool, error) {
	poolConfig, err := pgxpool.ParseConfig(cfg.DSN())
	if err != nil {
		return nil, fmt.Errorf("failed to parse database config: %w", err)
	}

	poolConfig.MaxConns = int32(cfg.MaxConnections)
	poolConfig.MinConns = int32(cfg.MaxIdleConnections)

	pool, err := pgxpool.NewWithConfig(ctx, poolConfig)
	if err != nil {
		return nil, fmt.Errorf("failed to create connection pool: %w", err)
	}

	if err := pool.Ping(ctx); err != nil {
		return nil, fmt.Errorf("failed to ping database: %w", err)
	}

	return pool, nil
}

type Repository struct {
	pool *pgxpool.Pool
}

func WrapPool(pool *pgxpool.Pool, cfg config.RepositoryConfig) *Repository {
	return &Repository{pool}
}

func (r* Repository) AddEvent(ctx context.Context, req *models.PostEventRequest) error {

	op_id, err := r.getOperationId(ctx, req.Op)
	if err != nil {
		return fmt.Errorf("pg AddEvent error: %w", err)
	}

	user_id, err := r.getUserId(ctx, req.User)
	if err != nil {
		return fmt.Errorf("pg AddEvent error: %w", err)
	}

	component_id, err := r.getComponentId(ctx, req.Component)
	component_id_ref := &component_id
	if err != nil && !errors.As(err, &eNullRes) {
		return fmt.Errorf("pg AddEvent error: %w", err)
	} else if errors.As(err, &eNullRes) {
		component_id_ref = nil
	}

	var session_id_ref *int64 = nil
	if req.Session_id != 0 {
		session_id_ref = &req.Session_id
	}

	var req_id_ref *int64 = nil
	if req.Req_id != 0 {
		req_id_ref = &req.Req_id
	}

	query := `
		INSERT INTO events (timestamp, user_id, op_id, component_id, session_id, req_id, res, attrs)
		VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
		RETURNING id
	`

	var event_id int64
	err = r.pool.QueryRow(ctx, query,
		req.Ts,
		user_id,
		op_id,
		component_id_ref,
		session_id_ref,
		req_id_ref,
		req.Res,
		req.Attrs,
	).Scan(&event_id)

	if err != nil {
		return fmt.Errorf("pg AddEvent query error: %w", err)
	}

	return nil
}

func (r* Repository) GetEvents(ctx context.Context, req *models.GetEventsQueryRequest) ([]*models.Event, error) {
	return nil, nil
}

type errorNullRes struct {}
func (e* errorNullRes) Error() string { return "Null result" }
var eNullRes *errorNullRes

func (r* Repository) getOperationId(ctx context.Context, op string) (int64, error) {
	return r.getOrCreateId(ctx, op, "operations")
}

func (r* Repository) getUserId(ctx context.Context, user string) (int64, error) {
	return r.getOrCreateId(ctx, user, "users")
}

func (r* Repository) getComponentId(ctx context.Context, component string) (int64, error) {
	return r.getOrCreateId(ctx, component, "components")
}

// Create if not presented
func (r* Repository) getOrCreateId(ctx context.Context, name string, table string) (int64, error) {
	tx, err := r.pool.Begin(ctx)
	if err != nil {
		select {
		case <-ctx.Done():
			return 0, fmt.Errorf("getOrCreateId context cancelled: %w", ctx.Err())
		default:
			tx.Commit(ctx)
			return 0, fmt.Errorf("getOrCreateId tx begin error: %w", err)
		}
	}

	var id int64
	query := fmt.Sprintf("SELECT id FROM %s WHERE name = $1", table) 
	err = tx.QueryRow(ctx, query, name).Scan(&id)
	if err == nil {
		err = tx.Commit(ctx) 
		if err != nil {
			select {
			case <-ctx.Done():
				return 0, fmt.Errorf("getComponentId context cancelled: %w", ctx.Err())
			default:
				return 0, fmt.Errorf("getComponentId tx commit error: %w", err)
			}
		}
		return id, nil
	}

	if err != pgx.ErrNoRows {
		select {
		case <-ctx.Done():
			return 0, fmt.Errorf("getOrCreateId context cancelled: %w", ctx.Err())
		default:
			tx.Commit(ctx)
			return 0, fmt.Errorf("getOrCreateId tx query error: %w", err)
		}
	}

	query = fmt.Sprintf("INSERT INTO %s (name) VALUES ($1) ON CONFLICT (name) DO NOTHING RETURNING id", table)
	err = tx.QueryRow(ctx, query, name).Scan(&id)
	if err != nil {
		select {
		case <-ctx.Done():
			return 0, fmt.Errorf("getOrCreateId context cancelled: %w", ctx.Err())
		default:
			tx.Rollback(ctx)
			return 0, fmt.Errorf("getOrCreateId tx insert error: %w", err)
		}
	}

	err = tx.Commit(ctx)
	if err != nil {
		select {
		case <-ctx.Done():
			return 0, fmt.Errorf("getComponentId context cancelled: %w", ctx.Err())
		default:
			return 0, fmt.Errorf("getComponentId tx commit error: %w", err)
		}
	}

	return id, nil
}