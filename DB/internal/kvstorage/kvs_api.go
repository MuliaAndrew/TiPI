package kvstorage

import (
	"github.com/recoilme/pudge"
)

// key-value storage is simplification of pudge kv storage
// we only need a single table in which we will store all data
// and CRUD operations on it
type Kvs struct {
	db*			pudge.Db
	path		string
}

func (kvs *Kvs) Open(path string) error {
	kvs.path = path
	cfg := new(pudge.Config)
	cfg.SyncInterval = 5
	cfg.StoreMode = 1

	db, err := pudge.Open(path, cfg)
	if err != nil {
		return err
	}
	kvs.db = db
	return nil
} 

func (kvs *Kvs) Write(key any, value any) error {
	return kvs.db.Set(key, value)
} 

func (kvs *Kvs) Read(key any) (string, error) {
	val := new(string)
	err := kvs.db.Get(key, val)
	return *val, err
} 

func (kvs *Kvs) Delete(key any) error {
	return kvs.db.Delete(key)
} 

func (kvs *Kvs) Close() error {
	err := kvs.db.Close()
	kvs.db = nil
	return err
} 