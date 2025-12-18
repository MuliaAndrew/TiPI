package config

import (
	"fmt"
	"time"

	"github.com/ilyakaznacheev/cleanenv"
)

type Config struct {
	Server   ServerConfig   `yaml:"server"`
	Database DatabaseConfig `yaml:"database"`
	Logging  LoggingConfig  `yaml:"logging"`
}

type ServerConfig struct {
	Host         string        `yaml:"host" env:"SERVER_HOST" env-default:"localhost"`
	Port         int           `yaml:"port" env:"SERVER_PORT" env-default:"8080"`
	ReadTimeout  time.Duration `yaml:"read_timeout" env:"SERVER_READ_TIMEOUT" env-default:"5s"`
	WriteTimeout time.Duration `yaml:"write_timeout" env:"SERVER_WRITE_TIMEOUT" env-default:"10s"`
}

func (s ServerConfig) Address() string {
	return fmt.Sprintf("%s:%d", s.Host, s.Port)
}

type DatabaseConfig struct {
	Host               string `yaml:"host" env:"DB_HOST" env-default:"localhost"`
	Port               int    `yaml:"port" env:"DB_PORT" env-default:"5432"`
	Name               string `yaml:"name" env:"DB_NAME" env-default:"auditordb"`
	User               string `yaml:"user" env:"DB_USER" env-default:"postgres"`
	Password           string `yaml:"password" env:"DB_PASSWORD" env-default:"postgres123"`
	SSLMode            string `yaml:"sslmode" env:"DB_SSLMODE" env-default:"disable"`
	MaxConnections     int    `yaml:"max_connections" env:"DB_MAX_CONNECTIONS" env-default:"25"`
	MaxIdleConnections int    `yaml:"max_idle_connections" env:"DB_MAX_IDLE_CONNECTIONS" env-default:"5"`
}

type RepositoryConfig struct {}

func (d DatabaseConfig) DSN() string {
	return fmt.Sprintf(
		"postgres://%s:%s@%s:%d/%s?sslmode=%s",
		d.User, d.Password, d.Host, d.Port, d.Name, d.SSLMode,
	)
}

type LoggingConfig struct {
	Level  string `yaml:"level" env:"LOG_LEVEL" env-default:"info"`
	Format string `yaml:"format" env:"LOG_FORMAT" env-default:"json"`
}

func Load(path string) (*Config, error) {
	var cfg Config

	if err := cleanenv.ReadConfig(path, &cfg); err != nil {
		return nil, fmt.Errorf("failed to read config: %w", err)
	}

	return &cfg, nil
}

func MustLoad(path string) *Config {
	cfg, err := Load(path)
	if err != nil {
		panic(err)
	}
	return cfg
}