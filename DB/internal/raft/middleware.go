package raft

import (
	"context"
	"log"
	"maps"
	"net"
	"net/http"
	"sync"
	"sync/atomic"
	"time"

	"DB/internal/kvstorage"
	"DB/internal/logger"
)

// replication states
const(
	MASTER uint32 = iota
	SLAVE
	CONDIDATE
)

const(
	HEARTBEAT_TOUT   time.Duration = 5000 * time.Millisecond
	HEARTBEAT_FREQ   time.Duration = 2000 * time.Millisecond
)

/*
	middleware is singleton holding all shared structures and variables for goroutines.
	it is designed to be accessible cuncurently without restrictions
*/
type MiddleWare struct {

	// general structures
	RaftLog     *logger.Logger // Raft threadsafe logger
	Kvs         *kvstorage.Kvs // threadsafe key-value storage
	SelfIp      net.IP // readonly
	OtherIp     []net.IP // readonly
	Log         *log.Logger // Server routines threadsafe logger

	// raft structures
	Term        uint64	// election term, atomic
	State       uint32	// replica state, either master, slave or condidate, atomic
	LeaderIP    net.IP	// leader ip address to redirect clients not-read requests, threadunsafe

	// used as mutex on three variables below
	// to notify handlers that CommitLen[req.ip] become 0 so slave had updated log and can be used
	Mu          *sync.Mutex
	Cond        *sync.Cond 
	VotedFor    net.IP	// used by HandleVoteFor to read and store our favorite condidate
	SentLen     map[string]uint64	// stores last replicated log index on slaves
	CommitLen   map[string]uint64 // stores raft log length (last log index) on slaves 
	
	// Slave, Election or Master  timer.
	// When used by slave, expired in `rand(2/3*HEARTBEAT_TOUT, 4/3*HEARTBEAT_TOUT)` and slave assume master is dead.
	// When used by master, expired in `HEARTBEAT_FREQ` send heartbeat massage to all slaves.
	// When used by condidate, expired in `rand(2/3*HEARTBEAT_TOUT, 4/3*HEARTBEAT_TOUT) and starts new election term.
	Timer       *time.Timer	// used only by heartbeat gorutines, which are exclusive
	
	// cancel function which terminates context of _co functions
	// it is not necessary to have exclusive access to Co_done_ctx when setting up new cancel func 
	Co_done_ctx     context.CancelFunc
}

type Rsp struct {
	ip     net.IP
	len    uint64
	term   uint64
	err    error
	ans    bool
}

type HttpHandler func(w http.ResponseWriter, req *http.Request, mw *MiddleWare)

// goroutine waits when timer expired and change state to condidate on expiration,
// used by slaves
func Heartbeat_consumer_co(mw *MiddleWare, ctx context.Context, co_done_ch chan<- struct{}) {
	select {
	case <-mw.Timer.C: // Heartbeat timer is reset on HandleHeartbeat
		// timer expired
		mw.Log.Printf("Heartbeat timeout expired, current term=(%d)", atomic.LoadUint64(&mw.Term))
		atomic.StoreUint32(&mw.State, CONDIDATE)
	case <-ctx.Done():
		mw.Log.Printf("Heartbeat timeout external interruption, current term=(%d)", atomic.LoadUint64(&mw.Term))
		// external canceleration
	}
	close(co_done_ch)
}

// gorountine as master periodically sends heartbeats to peers
func Heartbeat_producer_co(mw *MiddleWare, ctx context.Context, co_done_ch chan<- struct{}) {
	for i := uint64(0); true; i++ {
		select {
		case <-ctx.Done(): // interruption from outside
			return 
		// begin iteration on heartbeat sending
		// Handlers can reset timer if they want to immediately apply their changes
		case <-mw.Timer.C: 
		}
		
		mw.Timer.Reset(HEARTBEAT_FREQ)

		// we had request on election start with higher term on peers
		// or logic below consider we are not the master now. 
		// We need to wait at most HEARTBEAT_FREQ duration to react to ms.State change,
		// but this is totally fine in Raft algorithm
		if atomic.LoadUint32(&mw.State) != MASTER {
			close(co_done_ch)
			return
		}
		term := atomic.LoadUint64(&mw.Term)

		// starting hew i-th heartbeat session
		// wait until N+1 peers out of 2N+1 respond
		// if less then N+1 respond, we stop being master to not to
		// work in invalid cluster state (less then half replicas running)
		quorum_ctx, cancel := context.WithTimeout(ctx, HEARTBEAT_TOUT)
		defer cancel()
		go func(ctx context.Context) {

			// obtain info about peers raft log to update it with heartbeats if neaded
			mw.Cond.L.Lock()
			commits_to_replicate := mw.RaftLog.Ops(0)
			commits_to_apply := mw.RaftLog.UncommitedOps()

			commitLen := maps.Clone(mw.CommitLen)
			sentLen := maps.Clone(mw.SentLen)

			// buffered channel to count responses
			resp_ch := make(chan Rsp, len(mw.OtherIp))
			peers_log_updated := false
			for _, peer := range mw.OtherIp {

				// Peers last commit is older then ours. Send commit and apply it.
				// May interfere with Handle functions, but its okey to get resp with error,
				// we will try again on next iteration 
				if commitLen[peer.String()] < mw.RaftLog.LenCommited() && i != 0 {
					if uint64(len(commits_to_replicate)) != mw.RaftLog.LenCommited() {
						mw.Log.Fatalf("len(commits_to_replicate) (%d) != (%d) mw.RaftLog.LenCommited()", uint64(len(commits_to_replicate)), mw.RaftLog.LenCommited())
					}
					log := commits_to_replicate[commitLen[peer.String()]]
					mw.Log.Printf("logcommit {.term = %d, .ind = %d} nr=(%d) to (%s): send", term, log.Ind, i, peer)
					go SendReplicate(
						MEAN_LOGCOMMIT, 
						peer, 
						atomic.LoadUint64(&mw.Term), 
						log, 
						resp_ch,
					)

				// Peers last not applied log is older then ours. Send commit and apply it.
				// May interfere with Handle functions, but its okey to get resp with error,
				// we will try again on next iteration 
				} else if sentLen[peer.String()] < mw.RaftLog.LenCommited() && i != 0 {
					log := commits_to_replicate[sentLen[peer.String()]]
					mw.Log.Printf("logcommit {.term = %d, .ind = %d} nr=(%d) to (%s): send", term, log.Ind, i, peer)
					go SendReplicate(
						MEAN_LOGCOMMIT, 
						peer, 
						atomic.LoadUint64(&mw.Term), 
						log, 
						resp_ch,
					)

				} else if sentLen[peer.String()] < mw.RaftLog.LenUncommited() && i != 0 {
					log := commits_to_apply[sentLen[peer.String()] - mw.RaftLog.LenCommited()]
					mw.Log.Printf("logappend {.term = %d, .ind = %d} nr=(%d) to (%s): send", term, log.Ind, i, peer)
					go SendReplicate(
						MEAN_LOGAPPEND, 
						peer, 
						atomic.LoadUint64(&mw.Term), 
						log, 
						resp_ch,
					)
				} else {
					// all logs are up to date on this peer, send just a heartbeat
					mw.Log.Printf("heartbeat {.term = %d, .log_len = %d} nr=(%d) to (%s): send", term, mw.RaftLog.LenCommited(), i, peer)
					// sending our last log to peer with heartbeat 
					go SendReplicate(MEAN_HEARTBEAT, peer, atomic.LoadUint64(&mw.Term), mw.RaftLog.LastOp(), resp_ch)
				}
			}
			mw.Cond.L.Unlock()
			nr_responses := 0
			for nr_responses < len(mw.OtherIp) {
				select {
				case <-ctx.Done():
					if nr_responses < (len(mw.OtherIp) + 1) / 2 {
						// quorum has not been collected, stop being master to force election
						// which will be done when most of peers will be running
						mw.Log.Printf("Quorum on heartbeat session nr=(%d) did not collected, heartbeat tout had expired", i)
						atomic.StoreUint32(&mw.State, SLAVE)
						return
					} else {
						// ok quorum collected on heartbeat
						goto collected
					}
				case resp := <-resp_ch:
					if resp.err == nil {
						mw.Log.Printf("heartbeat nr=(%d) to (%s): response = {.len = %d, .ack = %t }", i, resp.ip.String(), resp.len, resp.ans)
						nr_responses++
						if resp.ans == false && resp.len <= mw.RaftLog.LenUncommited()  {
							mw.Cond.L.Lock()
							peers_log_updated = true
							mw.SentLen[resp.ip.String()] = max(resp.len, mw.SentLen[resp.ip.String()])
							mw.Cond.L.Unlock()
						} else if resp.ans == true && resp.len <= mw.RaftLog.LenCommited() {
							mw.Cond.L.Lock()
							peers_log_updated = true
							mw.CommitLen[resp.ip.String()] = max(resp.len, mw.CommitLen[resp.ip.String()])
							mw.SentLen[resp.ip.String()] = max(resp.len, mw.SentLen[resp.ip.String()])
							mw.Cond.L.Unlock()
						}
					} 
				}
			}
		collected:
			mw.Log.Printf("Quorum on heartbeat session nr=(%d) collected", i)
			if peers_log_updated {
				mw.Cond.Broadcast() // notify handlers that 
			}
		}(quorum_ctx)
		// from resp_ch 
		recover()
	}
}

// election_co called when self or another node starts election
func Election_co(mw *MiddleWare, ctx context.Context, co_done_ch chan<- struct{}) {
	term := atomic.AddUint64(&mw.Term, 1)
	vote_ch := make(chan Rsp, len(mw.OtherIp) / 2)
	timer := time.NewTimer(HEARTBEAT_TOUT + throttling())

	mw.Log.Printf("Election session for term (%d) started", term)
	for _, peer := range mw.OtherIp {
		mw.Log.Printf("SendVoteRequest with elTerm (%d) and logTerm (%d) to peer (%s)", term, mw.RaftLog.LenCommited(), peer)
		go SendVoteRequest_co(peer, mw.RaftLog.LenCommited(), mw.RaftLog.LastOp().Term, term, vote_ch)
	}

	votes := make(map[string]bool)
	votes[mw.SelfIp.String()] = true
	// collecting quorum on votes
	for len(votes) < len(mw.OtherIp) / 2 + 1 { 
		select {
		case resp := <- vote_ch:
			if resp.term == term && resp.ans == true {
				votes[resp.ip.String()] = true
				mw.Log.Printf("VoteResponse from (%s): peer vote for us", resp.ip)
			} else if resp.term == term {
				// there another condidate in our term, but its fine
				mw.Log.Printf("VoteResponse from (%s): peer vote for another condidate in this term", resp.ip)
			}
		case <-ctx.Done():
			// external canceleration 
			co_done_ch<- struct{}{}
			return
		case <-timer.C:
			// just return, in main loop Election_co will be called again with new election term
			mw.Log.Printf("Election session for term (%d) done without verdict", term)
			co_done_ch<- struct{}{}
			return
		}
	}

	mw.Cond.L.Lock()
	defer mw.Cond.L.Unlock()
	// quorum collected, become master and reset timer
	atomic.StoreUint32(&mw.State, MASTER)
	mw.LeaderIP = mw.SelfIp
	mw.Timer.Reset(time.Microsecond)
	mw.Log.Printf("Election session for term (%d) done", term)
	co_done_ch<- struct{}{}
	return
}