/*
 * tickets.h
 *
 *  Created on: Dec 18, 2018
 *  Author: yaopengfei(yuitta@163.com)
 *	this file is ported from decred tickets.go
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef BITCOIN_STAKE_TICKETS_H_
#define BITCOIN_STAKE_TICKETS_H_

#include <consensus/params.h>
#include <stake/tickettreap/common.h>
#include <stake/ticketdb/chainio.h>
#include <stake/lottery.h>
#include <primitives/block.h>

#include <vector>

using valtype = std::vector<unsigned char>;

const uint16_t FINAL_STATE_NUM = 6;

class TicketNode;

// Node is in-memory stake data for a node.  It contains a list of database
// updates to be written in the case that the block is inserted in the main
// chain database.  Because of its use of immutable treap data structures, it
// allows for a fast, efficient in-memory representation of the ticket database
// for each node.  It handles connection of and disconnection of new blocks
// simply.
//
// Like the immutable treap structures, stake nodes themselves are considered
// to be immutable.  The connection or disconnection of past or future nodes
// returns a pointer to a new stake node, which must be saved and used
// appropriately.
class TicketNode {

public:
	TicketNode() { SetNull(); }
	explicit TicketNode(const Consensus::Params& params): params(params) { SetNull(); }
	explicit TicketNode(TicketNode& node): height(node.height), liveTickets(node.liveTickets), missedTickets(node.missedTickets), revokedTickets(node.revokedTickets), params(node.params) {}
	explicit TicketNode(const TicketNode& node): height(node.height), liveTickets(node.liveTickets), missedTickets(node.missedTickets), revokedTickets(node.revokedTickets), params(node.params) {}

	void SetNull();

	bool IsNull(){
		return height == -1;
	}

	// UndoData returns the stored UndoTicketDataSlice used to remove this node
	// and restore it to the parent state.
	std::vector<UndoTicketData> UndoData() const{
		return databaseUndoUpdate;
	}

	// NewTickets returns the stored UndoTicketDataSlice used to remove this node
	// and restore it to the parent state.
	TicketHashes NewTickets() const{
		return databaseBlockTickets;
	}

	// SpentByBlock returns the tickets that were spent in this block.
	bool SpentByBlock(TicketHashes& hashes){
		for(uint16_t num; num < databaseUndoUpdate.size(); num++){
			if(databaseUndoUpdate[num].flag & TICKET_STATE_SPENT){
				hashes.push_back(databaseUndoUpdate[num].ticketHash);
			}
		}
		if(hashes.size()) return true;
		return false;
	}

	// MissedByBlock returns the tickets that were missed in this block. This
	// includes expired tickets and winning tickets that were not spent by a vote.
	// Also note that when a miss is later revoked, that ticket hash will also
	// appear in the output of this function for the block with the revocation.
	bool MissedByBlock(TicketHashes& hashes){
		for(uint16_t num; num < databaseUndoUpdate.size(); num++){
			if(databaseUndoUpdate[num].flag & TICKET_STATE_MISSED){
				hashes.push_back(databaseUndoUpdate[num].ticketHash);
			}
		}
		if(hashes.size()) return true;
		return false;
	}

	// ExpiredByBlock returns the tickets that expired in this block. This is a
	// subset of the missed tickets returned by MissedByBlock. The output only
	// includes the initial expiration of the ticket, not when an expired ticket is
	// revoked. This is unlike MissedByBlock that includes the revocation as well.
	bool ExpiredByBlock(TicketHashes& hashes){
		for(uint16_t num; num < databaseUndoUpdate.size(); num++){
			if(databaseUndoUpdate[num].flag & TICKET_STATE_EXPIRED){
				hashes.push_back(databaseUndoUpdate[num].ticketHash);
			}
		}
		if(hashes.size()) return true;
		return false;
	}

	// ExistsLiveTicket returns whether or not a ticket exists in the live ticket
	// treap for this stake node.
	bool ExistsLiveTicket(uint256& hash){
		return liveTickets.Has(hash);
	}

	// LiveTickets returns the list of live tickets for this stake node.
	bool LiveTickets(TicketHashes& hashes){
		liveTickets.ForEach(hashes);
		if(hashes.size()) return true;
		return false;
	}

	// PoolSize returns the size of the live ticket pool.
	int64_t PoolSize(){
		return liveTickets.Len();
	}

	// ExistsMissedTicket returns whether or not a ticket exists in the missed
	// ticket treap for this stake node.
	bool ExistsMissedTicket(uint256& hash){
		return missedTickets.Has(hash);
	}

	// MissedTickets returns the list of missed tickets for this stake node.
	bool MissedTickets(TicketHashes& hashes){
		missedTickets.ForEach(hashes);
		if(hashes.size()) return true;
		return false;
	}

	// ExistsRevokedTicket returns whether or not a ticket exists in the revoked
	// ticket treap for this stake node.
	bool ExistsRevokedTicket(uint256& hash){
		return revokedTickets.Has(hash);
	}

	// RevokedTickets returns the list of revoked tickets for this stake node.
	bool RevokedTickets(TicketHashes& hashes){
		revokedTickets.ForEach(hashes);
		if(hashes.size()) return true;
		return false;
	}

	// ExistsExpiredTicket returns whether or not a ticket was ever expired from
	// the perspective of this stake node.
	bool ExistsExpiredTicket(uint256& hash){
		treapNode* node = missedTickets.Get(hash);
		if(node->mflag & TICKET_STATE_EXPIRED) return true;
		node = revokedTickets.Get(hash);
		if(node->mflag & TICKET_STATE_EXPIRED) return true;

		return false;
	}

	// Winners returns the current list of winners for this stake node, which
	// can vote on this node.
	TicketHashes Winners(){
		return nextwinners;
	}

	// FinalState returns the final state lottery checksum of the node.
	unsigned char* FinalState(){
		return finalState;
	}

	// Height returns the height of the node.
	uint32_t Height(){
		return height;
	}

	// genesisNode returns a pointer to the initialized ticket database for the
	// genesis block.
	bool genesisNode(const Consensus::Params& paramsIn){
		height = 0;
		params = paramsIn;
		return true;
	}

	bool ForEachByHeight(uint32_t heightLessThan);

	friend bool InitDatabaseState(const Consensus::Params& params, TicketNode& node);
	friend bool LoadBestNode(uint32_t height, uint256& blockhash, CBlockHeader& header, const Consensus::Params params, TicketNode& node);
	friend bool connectNode(TicketNode& node, uint256 lotteryIV, TicketHashes& ticketsVoted, TicketHashes& revokedTickets, TicketHashes& newTickets, TicketNode& nodeOut);
	friend bool disconnectNode(TicketNode& node, uint256 parentLotteryIV, std::vector<UndoTicketData>& parentUtds, TicketHashes& parentTickets, TicketNode& nodeOut);
	friend bool WriteConnectedBestNode(TicketNode& node, const uint256& hash);
	friend bool WriteDisconnectedBestNode(TicketNode& node, uint256& hash, std::vector<UndoTicketData>& childUndoData);

private:
	int64_t height;

	// liveTickets is the treap of the live tickets for this node.
	Immutable liveTickets;

	// missedTickets is the treap of missed tickets for this node.
	Immutable missedTickets;

	// revokedTickets is the treap of revoked tickets for this node.
	Immutable revokedTickets;

	// databaseUndoUpdate is the cache of the database used to undo
	// the current node's addition to the blockchain.
	std::vector<UndoTicketData> databaseUndoUpdate;

	// databaseBlockTickets is the cache of the new tickets to insert
	// into the database.
	TicketHashes databaseBlockTickets;

	// nextWinners is the list of the next winners for this block.
	// into the database.
	TicketHashes nextwinners;

	// finalState is the calculated final state checksum from the live
	// tickets pool.
	unsigned char finalState[FINAL_STATE_NUM];

	// params is the blockchain parameters.
	Consensus::Params params;
};

bool InitDatabaseState(const Consensus::Params& params, TicketNode& node);
bool LoadBestNode(uint32_t height, uint256& blockhash, CBlockHeader& header, const Consensus::Params params, TicketNode& node);
bool safeGet(Immutable& imu, uint256& hash, uint32_t*& pheight, uint8_t*& pflag);
bool safePut(Immutable& imu, uint256& hash, uint32_t& height, uint8_t& flag);
bool safeDelete(Immutable& imu, uint256& hash);
bool connectNode(TicketNode& node, uint256 lotteryIV, TicketHashes& ticketsVoted, TicketHashes& revokedTickets, TicketHashes& newTickets, TicketNode& nodeOut);
bool disconnectNode(TicketNode& node, uint256 parentLotteryIV, std::vector<UndoTicketData>& parentUtds, TicketHashes& parentTickets, TicketNode& nodeOut);
bool WriteConnectedBestNode(TicketNode& node, const uint256& hash);
bool WriteDisconnectedBestNode(TicketNode& node, uint256& hash, std::vector<UndoTicketData>& childUndoData);

#endif /* BITCOIN_STAKE_TICKETS_H_ */
