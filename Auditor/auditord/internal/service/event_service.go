package service

import (
	"context"
	"fmt"

	"github.com/MuliaAndrew/TiPI/Auditor/auditord/internal/models"
)

type EventRepository interface {
	AddEvent(ctx context.Context, req *models.PostEventRequest) error
	GetEvents(ctx context.Context, req *models.GetEventsQueryRequest) ([]*models.Event, error)
}

type EventService struct {
	repo EventRepository
}

func NewEventService(repo EventRepository) *EventService {
	return &EventService{repo: repo}
}

func (es *EventService) PostEvent(ctx context.Context, req *models.PostEventRequest) (*models.PostEventResponse, error) {
	if err := es.repo.AddEvent(ctx, req); err != nil {
		return nil, fmt.Errorf("failed to post event: %w", err)
	}

	return &models.PostEventResponse{}, nil
}

func (es EventService) GetEventsQuery(ctx context.Context, req *models.GetEventsQueryRequest) (*models.GetEventsQueryResponse, error) {
	resp, err := es.repo.GetEvents(ctx, req)
	if err != nil {
		return nil, fmt.Errorf("failed to query event: %w", err)
	}

	if resp == nil {
		resp = []*models.Event{}
	}

	return &models.GetEventsQueryResponse{
		Events: resp,
	}, nil
}