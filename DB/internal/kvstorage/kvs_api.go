package kvstorage;

import (
	"github.com/recoilme/pudge"
)

// key-value storage is simplification of pudge kv storage
// we only need a single table in which we will store all data
// and CRUD operations on it
type Kvs struct {
	db			pudge.Db
	
}

func (kvs *Kvs) Open() error {
	return nil
} 

func (kvs *Kvs) Write() error {
	return nil
} 

func (kvs *Kvs) Read() error {
	return nil
} 

func (kvs *Kvs) Delete() error {
	return nil
} 

func (kvs *Kvs) Close() error {
	return nil
} 