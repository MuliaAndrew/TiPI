package main

import (
	"database/sql"
	_ "github.com/jackc/pgx/v5/stdlib"
	"fmt"
	"log/slog"
	"os"
)

var (
	db_name    string
	db_user    string
	db_pass    string
	db_ssl     string

	log_level  int
)

func parse_args() {
	db_name = os.Getenv("DB_NAME")
	db_user = os.Getenv("DB_USER")
	db_pass = os.Getenv("DB_PASS")
	db_ssl  = os.Getenv("DB_SSL")
	
	if db_name == "" {
		slog.Error("DB_NAME env variable must be set")
	}
	if db_user == "" {
		slog.Error("DB_USER env variable must be set")
	}
	if db_pass == "" {
		slog.Error("DB_PASS env variable must be set")
	}
	if db_ssl == "" { 
		db_ssl = "disable"
	}
}

func main() {
	
	parse_args()
	slog.SetLogLoggerLevel(slog.Level(log_level))

	connStr := fmt.Sprintf("user=%s password=%s dbname=%s sslmode=%s host=localhost port=5432", db_user, db_pass, db_name, db_ssl)
	db, err := sql.Open("pgx", connStr)
	if err != nil {
		slog.Error("Cant connect to database", "err", err.Error())
		os.Exit(1)
	}
	defer db.Close()

	rows, err := db.Query("SELECT city, tm FROM test")
	if err != nil {
		slog.Error("Cant query", "err", err.Error())
		os.Exit(1)
	}
	defer rows.Close()

	var data struct {
		city string
		tm   string
	}

	for rows.Next() {
		err = rows.Scan(&data.city, &data.tm)
		if err != nil {
			slog.Error("Cant scan for rows", "err", err.Error())
			break
		}
		fmt.Printf("city: %s, timestamp: %s\n", data.city, data.tm)
	}
}