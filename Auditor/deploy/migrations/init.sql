CREATE TABLE users (
  id BIGSERIAL PRIMARY KEY,
  name VARCHAR(255) NOT NULL UNIQUE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_users_name ON users(name);

CREATE TABLE components (
  id BIGSERIAL PRIMARY KEY,
  name VARCHAR(255) NOT NULL UNIQUE,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
  updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_component_name ON components(name);

CREATE TABLE operations (
  id BIGSERIAL PRIMARY KEY,
  name VARCHAR(255) NOT NULL UNIQUE
);

CREATE INDEX idx_operation_name ON operations(name);

CREATE TABLE events (
  id BIGSERIAL PRIMARY KEY,
  timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
  user_id BIGINT NOT NULL,
  component_id BIGINT,
  op_id BIGINT NOT NULL,
  session_id BIGINT,
  req_id BIGINT,
  res JSONB,
  attrs JSONB,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
  
  CONSTRAINT fk_events_user 
    FOREIGN KEY (user_id) 
    REFERENCES users(id) 
    ON DELETE RESTRICT,
  
  CONSTRAINT fk_events_component 
    FOREIGN KEY (component_id) 
    REFERENCES components(id) 
    ON DELETE RESTRICT,
  
  CONSTRAINT fk_events_op
    FOREIGN KEY (op_id) 
    REFERENCES operations(id) 
    ON DELETE RESTRICT
);