/*
 * tickets.cpp
 *
 *  Created on: Dec 18, 2018
 *  Author: yaopengfei(yuitta@163.com)
 *	this file is ported from decred tickets.go
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <stake/tickets.h>
#include <crypto/sha256.h>
#include <stakedb.h>

#include <util.h>

// TODO false add content

// InitDatabaseState initializes the chain with the best state being the
// genesis block.
bool InitDatabaseState(const Consensus::Params& params, TicketNode& node){
	// Write the new block undo and new tickets data to the
	// database for the genesis block.
	node.genesisNode(params);

	// Write the new block undo and new tickets data to the
	// database for the genesis block.
	pdbinfoview->writeUndoTicketData(node.height, node.databaseUndoUpdate);	// TODO

	pdbinfoview->writeTicketHashes(node.height, node.databaseBlockTickets);

	// Write the new best state to the database.
	TicketHashes NextWinners;
	NextWinners.resize(node.params.TicketsPerBlock);

	// Write the new best state to the database.
	const BestChainState bcState = {
		.hash = params.hashGenesisBlock,
		.height = node.height,
		.live = (uint32_t)node.liveTickets.Len(),
		.missed = (uint64_t)node.missedTickets.Len(),
		.revoked = (uint64_t)node.revokedTickets.Len(),
		.perblock = node.params.TicketsPerBlock,
		.nextwinners = NextWinners
	};
	if(pdbinfoview->writeBestChainState(bcState) == false) return error("%s: writeBestChainState failed", __func__);

	return true;
}

// LoadBestNode is used when the blockchain is initialized, to get the initial
// stake node from the database bucket.  The blockchain must pass the height
// and the blockHash to confirm that the ticket database is on the same
// location in the blockchain as the blockchain itself.  This function also
// checks to ensure that the database has not failed the upgrade process and
// reports the current version.  Upgrades are also handled by this function,
// when they become applicable.
bool LoadBestNode(uint32_t height, uint256& blockhash, CBlockHeader& header, const Consensus::Params params, TicketNode& node){
	DatabaseInfo dbInfo;
	if(pdbinfoview->readDatabaseInfo(dbInfo) == false) return error("%s: initial invoke readDatabaseInfo function failed", __func__);

	// Compare the tip and make sure it matches.
	BestChainState state;
	if(pdbinfoview->readBestChainState(state)) return error("%s: readBestChainState failed", __func__);

	if(state.hash != blockhash || state.height != height){
		return error("%s: best state corruption", __func__);
	}

	// Restore the best node treaps form the database.
	node.height = height;
	node.params = params;
	if(pliveticketview->readALLSingleTicketState(LiveTicketsBucketName, node.liveTickets) == false)
		return error("%s: live tickets readALLSingleTicketState failed", __func__);
	if(node.liveTickets.Len() != state.live){
		return error("%s: live tickets corruption (got %d in state but loaded %d)", __func__, state.live, node.liveTickets.Len());
	}

	if(pmissedticketview->readALLSingleTicketState(MissedTicketsBucketName, node.missedTickets) == false)
		return error("%s: missed tickets readALLSingleTicketState failed", __func__);
	if((uint64_t)node.missedTickets.Len() != state.missed){
		return error("%s: missed tickets corruption (got %d in state but loaded %d)", __func__, state.missed, node.missedTickets.Len());
	}

	if(prevokedticketview->readALLSingleTicketState(RevokedTicketsBucketName, node.revokedTickets) == false)
		return error("%s: revoked tickets readALLSingleTicketState failed", __func__);
	if((uint64_t)node.revokedTickets.Len() != state.revoked){
		return error("%s: revoked tickets corruption (got %d in state but loaded %d)", state.revoked, node.revokedTickets.Len());
	}

	// Restore the node undo and new tickets data.
	if(pdbinfoview->readUndoTicketData(height, node.databaseUndoUpdate) == false)
		return error("%s: readUndoTicketData on the %d height failed", __func__, height);

	if(pdbinfoview->readTicketHashes(height, node.databaseBlockTickets) == false)
		return error("%s: readTicketHashes on the %d height failed", __func__, height);

	// Restore the next winners for the node.
	if(node.height >= node.params.StakeValidationHeight-1){
		for(uint256 NextWinner : state.nextwinners){
			node.nextwinners.push_back(NextWinner);
		}

		// Calculate the final state from the block header.
		TicketHashes stateBuffer;
		for(auto hash : node.nextwinners){
			stateBuffer.push_back(hash);
		}

		const std::string headerHex = header.GetHex();
		Hash256PRNG prng = NewHash256PRNG(std::vector<unsigned char> (headerHex.begin(), headerHex.end()));
		std::vector<int32_t> list;
		bool res = findTicketIdxs(node.liveTickets.Len(), node.params.TicketsPerBlock, prng, list);
		if(res == false) return false;

		uint256 lastHash;
		prng.StateHash(lastHash);
		stateBuffer.push_back(lastHash);

		uint256 finalHash;
		CSHA256 hasher;
		for(uint16_t num = 0; num < (uint16_t)stateBuffer.size(); num++){
			hasher.Write(stateBuffer[num].begin(), 32);
		}
		hasher.Finalize(finalHash.begin());

		memcpy(node.finalState, finalHash.begin(), FINAL_STATE_NUM);
	}

	LogPrintf("Stake database version %d loaded", dbInfo.version);

	return true;
}

// safeGet fetches a pointer to the data for the key in the treap, then
// copies the value so that the original pointer to the key is never written
// to accidentally later.
// TODO This function could also check to make sure the states of the ticket
//       treap value are valid.
bool safeGet(Immutable& imu, uint256& hash, uint32_t*& pheight, uint8_t*& pflag){
	std::shared_ptr<treapNode> node = imu.Get(hash);
	if(node == nullptr){
		return error("%s: ticket %s was supposed to be in the passed treap, but could not be found", __func__, hash.GetHex());
	}
	pheight = &node->mheight;
	pflag = &node->mflag;
	return true;
}

// safeDelete is used to delete a value in an immutable ticket treap, returning
// the mutated, immutable treap given as a result.  It first checks to see if
// the key exists in the treap. If it does not, it returns an error.
bool safePut(Immutable& imu, uint256& hash, uint32_t& height, uint8_t& flag){
	if(imu.Has(hash)){
		return error("%s: attempted to insert duplicate key key %s from treap", __func__, hash.GetHex());
	}
	imu.Put(hash, height, flag);
	return true;
}

// safeDelete is used to delete a value in an immutable ticket treap, returning
// the mutated, immutable treap given as a result.  It first checks to see if
// the key exists in the treap. If it does not, it returns an error.
bool safeDelete(Immutable& imu, uint256& hash){
	if(!imu.Has(hash)){
		return error("%s: attempted to delete non-existing key %s from treap", __func__, hash.GetHex());
	}

	imu.Delete(hash);
	return true;
}

void TicketNode::SetNull(){
	height = -1;
	liveTickets = Immutable();
	missedTickets = Immutable();
	revokedTickets = Immutable();
	databaseUndoUpdate.clear();
	databaseBlockTickets.clear();
	nextwinners.clear();
}

// ForEachByHeight iterates all elements in the tree less than a given height in
// the blockchain.
bool TicketNode::ForEachByHeight(uint32_t heightLessThan){
	// Add the root node and all children to the left of it to the list of
	// nodes to traverse and loop until they, and all of their child nodes,
	// have been traversed.
	parentStack parents;
	for(std::shared_ptr<treapNode> node = liveTickets.mroot; node != nullptr && node->mpriority < heightLessThan; node = node->mleft){
		parents.Push(node);
	}
	while(parents.Len() > 0){
		std::shared_ptr<treapNode> pnode = parents.Pop();
		pnode->mflag |= TICKET_STATE_MISSED;
		pnode->mflag |= TICKET_STATE_EXPIRED;
		if(safeDelete(liveTickets, pnode->mkey) == false){
			return false;
		}

		if(safePut(missedTickets, pnode->mkey, pnode->mheight, pnode->mflag) == false){
			return false;
		}

		UndoTicketData unTD = {
				.ticketHash = pnode->mkey,
				.ticketHeight = pnode->mheight,
				.flag = pnode->mflag
		};
		databaseUndoUpdate.push_back(unTD);

		// Extend the nodes to traverse by all children to the left of
		// the current node's right child.
		for(std::shared_ptr<treapNode> nodeIn = pnode->mright; nodeIn != nullptr && nodeIn->mpriority < heightLessThan; nodeIn = nodeIn->mleft){
			parents.Push(nodeIn);
		}
	}
	return true;
}

// connectNode connects a child to a parent stake node, returning the
// modified stake node for the child.  It is important to keep in mind that
// the argument node is the parent node, and that the child stake node is
// returned after subsequent modification of the parent node's immutable
// data.
bool connectNode(TicketNode& node, uint256 lotteryIV, TicketHashes& ticketsVoted, TicketHashes& revokedTickets, TicketHashes& newTickets, TicketNode& nodeOut){
	nodeOut.height = node.height + 1;
	nodeOut.liveTickets = node.liveTickets;
	nodeOut.revokedTickets = node.revokedTickets;
	nodeOut.missedTickets = node.missedTickets;
	nodeOut.databaseBlockTickets = newTickets;
	nodeOut.params = node.params;

	TicketHashes::iterator it;

	// We only have to deal with vote-related issues and expiry after
	// StakeEnabledHeight.
	if(nodeOut.height >= (uint32_t)nodeOut.params.StakeEnabledHeight){
		// Basic sanity check.
		for(uint256 ticketvoted : ticketsVoted){
			it = std::find(node.nextwinners.begin(), node.nextwinners.end(), ticketvoted);
			if(it == node.nextwinners.end()){
				return error("%s: unknown ticket %s spent in block", __func__, ticketvoted.GetHex());
			}
		}

		// Iterate through all possible winners and construct the undo data,
		// updating the live and missed ticket treaps as necessary.  We need
		// to copy the value here so we don't modify it in the previous treap.
		for(uint256 ticket : node.nextwinners){
			uint32_t* uheight = nullptr;
			uint8_t* uflag = nullptr;
			if(safeGet(nodeOut.liveTickets, ticket, uheight, uflag) == false) return false;

			// If it's spent in this block, mark it as being spent.  Otherwise,
			// it was missed.  Spent tickets are dropped from the live ticket
			// bucket, while missed tickets are pushed to the missed ticket
			// bucket.  Because we already know from the above check that the
			// ticket should still be in the live tickets treap, we probably
			// do not have to use the safe delete functions, but do so anyway
			// just to be safe.
			it = std::find(ticketsVoted.begin(), ticketsVoted.end(), ticket);
			if(it != ticketsVoted.end()){
				*uflag |= TICKET_STATE_SPENT;
				*uflag &= ~TICKET_STATE_MISSED;
				if(safeDelete(nodeOut.liveTickets, ticket) == false){
					return false;
				}
			} else {
				*uflag &= ~TICKET_STATE_SPENT;
				*uflag |= TICKET_STATE_MISSED;
				if(safeDelete(nodeOut.liveTickets, ticket) == false){
					return false;
				}
				if(safePut(nodeOut.missedTickets, ticket, *uheight, *uflag) == false){
					return false;
				}
			}
			UndoTicketData undoTicket = {
				.ticketHash = ticket,
				.ticketHeight =  *uheight,
				.flag = *uflag
			};
			nodeOut.databaseUndoUpdate.push_back(undoTicket);
		}

		// Find the expiring tickets and drop them as well.  We already know what
		// the winners are from the cached information in the previous block, so
		// no drop the results of that here.
		uint32_t toExpireHeight = 0;
		if(nodeOut.height > nodeOut.params.TicketExpiry){
			toExpireHeight = nodeOut.height - nodeOut.params.TicketExpiry;
		}
		if(nodeOut.ForEachByHeight(toExpireHeight + 1) == false){
			return false;
		}

		// Process all the revocations, moving them from the missed to the
		// revoked treap and recording them in the undo data.
		for(uint256 revokedTicket : revokedTickets){
			uint32_t* pheight = nullptr;
			uint8_t* pflag = nullptr;
			if(safeGet(nodeOut.missedTickets, revokedTicket, pheight, pflag) == false){
				return false;
			}
			*pflag |= TICKET_STATE_REVOKED;
			if(safeDelete(nodeOut.missedTickets, revokedTicket) == false){
				return true;
			}
			if(safePut(nodeOut.revokedTickets, revokedTicket, *pheight, *pflag) == false){
				return false;
			}
			UndoTicketData undoTicket = {
				.ticketHash = revokedTicket,
				.ticketHeight =  *pheight,
				.flag = *pflag
			};
			nodeOut.databaseUndoUpdate.push_back(undoTicket);
		}
	}

	// Add all the new tickets.
	for(auto newTicket : newTickets){
		uint8_t newflag = 0;
		uint32_t heightCopy = (uint32_t)nodeOut.height;
		if(safePut(nodeOut.liveTickets, newTicket, heightCopy, newflag) == false){
			return false;
		}
		UndoTicketData undoTicket = {
			.ticketHash = newTicket,
			.ticketHeight =  (uint32_t)nodeOut.height,
			.flag = newflag
		};
		nodeOut.databaseUndoUpdate.push_back(undoTicket);
	}

	// The first block voted on is at StakeValidationHeight, so begin calculating
	// winners at the block before StakeValidationHeight.
	if(nodeOut.height >= uint32_t(nodeOut.params.StakeValidationHeight -1)){
		// Find the next set of winners.
		Hash256PRNG prng = NewHash256PRNGFromIV(lotteryIV);
		std::vector<int32_t> lis;
		if(findTicketIdxs(nodeOut.liveTickets.Len(), nodeOut.params.TicketsPerBlock, prng, lis) == false){
			return true;
		}
		TicketHashes stateBuffer, nextWinnersKeys;
		stateBuffer.resize(nodeOut.params.TicketsPerBlock+1);
		uint16_t index = 0;
		if(fetchWinners(lis, nodeOut.liveTickets, nextWinnersKeys) == false){
			return false;
		}

		for(auto treapKey : nextWinnersKeys){
			nodeOut.nextwinners.push_back(treapKey);
			stateBuffer[index] = treapKey;
			index++;
		}
		uint256 lastHash;
		prng.StateHash(lastHash);
		stateBuffer[index] = lastHash;

		uint256 finalHash;
		CSHA256 hasher;
		for(uint16_t num = 0; num < stateBuffer.size(); num++){
			hasher.Write(stateBuffer[num].begin(), 32);
		}
		hasher.Finalize(finalHash.begin());

		memcpy(nodeOut.finalState, finalHash.begin(), FINAL_STATE_NUM);
	}

	return true;
}

// disconnectNode disconnects a stake node from itself and returns the state of
// the parent node.  The database transaction should be included if the
// UndoTicketDataSlice or tickets are nil in order to look up the undo data or
// tickets from the database.
bool disconnectNode(TicketNode& node, uint256 parentLotteryIV, std::vector<UndoTicketData>& parentUtds, TicketHashes& parentTickets, TicketNode& nodeOut){
	// Edge case for the parent being the genesis block.
	if(node.height == 1){
		nodeOut.genesisNode(node.params);
		return true;
	}

	if(node.height == 0){
		return error("%s: missing stake node pointer input when disconnecting", __func__);
	}

	// The undo ticket slice is normally stored in memory for the most
	// recent blocks and the sidechain, but it may be the case that it
	// is missing because it's in the mainchain and very old (thus
	// outside the node cache).  In this case, restore this data from
	// disk.
	if(parentUtds.empty() || parentTickets.empty()){
		if(pdbinfoview->readUndoTicketData(node.height - 1, parentUtds) == false){
			return error("%s: invoke readUndoTicketData fail", __func__);
		}
		if(pdbinfoview->readTicketHashes(node.height - 1, parentTickets) == false){
			return error("%s: invoke readTicketHashes fail", __func__);
		}
	}

	nodeOut.height = node.height - 1;
	nodeOut.liveTickets = node.liveTickets;
	nodeOut.missedTickets = node.missedTickets;
	nodeOut.revokedTickets = node.revokedTickets;
	nodeOut.databaseUndoUpdate = parentUtds;
	nodeOut.databaseBlockTickets = parentTickets;
	nodeOut.params = node.params;

	// Iterate through the block undo data and write all database
	// changes to the respective treap, reversing all the changes
	// added when the child block was added to the chain.
	TicketHashes stateBuffer;
	stateBuffer.resize(node.params.TicketsPerBlock+1);
	uint16_t index = 0;
	for(auto undo : node.databaseUndoUpdate){
		// All flags are unset; this is a newly added ticket.
		// Remove it from the list of live tickets.
		if(!(undo.flag & TICKET_STATE_SPENT) && !(undo.flag & TICKET_STATE_REVOKED) && !(undo.flag & TICKET_STATE_MISSED)){
			if(safeDelete(nodeOut.liveTickets, undo.ticketHash) == false){
				return false;
			}
		}
		// The ticket was missed and revoked.  It needs to
		// be moved from the revoked ticket treap to the
		// missed ticket treap.
		else if((undo.flag & TICKET_STATE_MISSED) && (undo.flag & TICKET_STATE_REVOKED)){
			undo.flag &= ~TICKET_STATE_REVOKED;
			if(safeDelete(nodeOut.revokedTickets, undo.ticketHash) == false){
				return false;
			}
			if(safePut(nodeOut.missedTickets, undo.ticketHash, undo.ticketHeight, undo.flag) == false){
				return false;
			}
		}
		// The ticket was missed and was previously live.
		// Remove it from the missed tickets bucket and
		// move it to the live tickets bucket.
		else if((undo.flag & TICKET_STATE_MISSED) && !(undo.flag & TICKET_STATE_REVOKED)){
			// Expired tickets could never have been
			// winners.
			if(!(undo.flag & TICKET_STATE_EXPIRED)){
				nodeOut.nextwinners.push_back(undo.ticketHash);
				stateBuffer[index] = undo.ticketHash;
				index++;
			} else {
				undo.flag &= ~TICKET_STATE_EXPIRED;
			}
			undo.flag &= ~TICKET_STATE_MISSED;
			if(safeDelete(nodeOut.missedTickets, undo.ticketHash) == false){
				return false;
			}
			if(safePut(nodeOut.liveTickets, undo.ticketHash, undo.ticketHeight, undo.flag) == false){
				return false;
			}
		}

		// The ticket was spent.  Reinsert it into the live
		// tickets treap and add it to the list of next
		// winners.
		else if(undo.flag & TICKET_STATE_SPENT){
			undo.flag &= ~TICKET_STATE_EXPIRED;
			nodeOut.nextwinners.push_back(undo.ticketHash);
			stateBuffer[index] = undo.ticketHash;
			index++;
			if(safePut(nodeOut.liveTickets, undo.ticketHash, undo.ticketHeight, undo.flag) == false){
				return false;
			}
		}
		else{
			return error("%s: unknown ticket state in undo data", __func__);
		}
	}
	if(node.height >= (uint32_t)node.params.StakeValidationHeight){
		Hash256PRNG prng = NewHash256PRNGFromIV(parentLotteryIV);
		std::vector<int32_t> lis;
		if(findTicketIdxs(nodeOut.liveTickets.Len(), node.params.TicketsPerBlock, prng, lis) == false){
			return false;
		}
		uint256 lastHash;
		prng.StateHash(lastHash);
		stateBuffer[index] = lastHash;

		uint256 finalHash;
		CSHA256 hasher;
		for(uint16_t num = 0; num < (uint16_t)stateBuffer.size(); num++){
			hasher.Write(stateBuffer[num].begin(), 32);
		}
		hasher.Finalize(finalHash.begin());

		memcpy(nodeOut.finalState, finalHash.begin(), FINAL_STATE_NUM);
	}
	return true;
}

// WriteConnectedBestNode writes the newly connected best node to the database
// under an atomic database transaction, performing all the necessary writes to
// the database buckets for live, missed, and revoked tickets.
bool WriteConnectedBestNode(TicketNode& node, const uint256& hash){
	// Iterate through the block undo data and write all database
	// changes to the respective on-disk map.
	for(auto undo : node.databaseUndoUpdate){
		// All flags are unset; this is a newly added ticket.
		// Insert it into the live ticket database.
		if(!(undo.flag & TICKET_STATE_SPENT) && !(undo.flag & TICKET_STATE_REVOKED) && !(undo.flag & TICKET_STATE_MISSED)) {
			const singleTicketState sts = {
				.hash = undo.ticketHash,
				.height = undo.ticketHeight,
				.ticketstate = undo.flag
			};
			if(pliveticketview->writeSingleTicketState(LiveTicketsBucketName, sts) == false){
				return false;
			}
		}
		// The ticket was missed and revoked.  It needs to
		// be moved from the missed ticket bucket to the
		// revoked ticket bucket.
		else if((undo.flag & TICKET_STATE_MISSED) && (undo.flag & TICKET_STATE_REVOKED)){
			if(pmissedticketview->earseSingleTicketState(MissedTicketsBucketName, undo.ticketHash) == false){
				return false;
			}
			const singleTicketState sts = {
				.hash = undo.ticketHash,
				.height = undo.ticketHeight,
				.ticketstate = undo.flag
			};
			if(prevokedticketview->writeSingleTicketState(RevokedTicketsBucketName, sts)){
				return false;
			}
		}
		// The ticket was missed and was previously live.
		// Move it from the live ticket bucket to the missed
		// ticket bucket.
		else if((undo.flag & TICKET_STATE_MISSED) && !(undo.flag & TICKET_STATE_REVOKED)){
			if(pliveticketview->earseSingleTicketState(LiveTicketsBucketName, undo.ticketHash)){
				return false;
			}
			const singleTicketState sts = {
				.hash = undo.ticketHash,
				.height = undo.ticketHeight,
				.ticketstate = undo.flag
			};
			if(pmissedticketview->writeSingleTicketState(MissedTicketsBucketName, sts)){
				return false;
			}
		}
		// The ticket was spent.  Remove it from the live
		// ticket bucket.
		else if(undo.flag & TICKET_STATE_SPENT){
			if(pliveticketview->earseSingleTicketState(LiveTicketsBucketName, undo.ticketHash) == false){
				return false;
			}
		}
		else{
			return error("%s: unknown ticket state in undo data", __func__);
		}
	}

	// Write the new block undo and new tickets data to the
	// database for the given height, potentially overwriting
	// an old entry with the new data.
	if(pdbinfoview->writeUndoTicketData(node.height, node.databaseUndoUpdate) == false){
		return false;
	}

	if(pdbinfoview->writeTicketHashes(node.height, node.databaseBlockTickets) == false){
		return false;
	}

	// Write the new best state to the database.
	TicketHashes nextWinners;
	nextWinners.resize(node.params.TicketsPerBlock);
	if(node.height >= uint32_t(node.params.StakeValidationHeight -1)){
		for(uint16_t num = 0; num < node.nextwinners.size(); num++){
			nextWinners[num] = node.nextwinners[num];
		}
	}
	const BestChainState bcs = {
		.hash = hash,
		.height = node.height,
		.live = (uint32_t)node.liveTickets.Len(),
		.missed = (uint64_t)node.missedTickets.Len(),
		.revoked = (uint64_t)node.revokedTickets.Len(),
		.perblock = node.params.TicketsPerBlock,
		.nextwinners = nextWinners
	};
	return pdbinfoview->writeBestChainState(bcs);

}

// WriteDisconnectedBestNode writes the newly connected best node to the database
// under an atomic database transaction, performing all the necessary writes to
// reverse the contents of the database buckets for live, missed, and revoked
// tickets.  It does so by using the parent's block undo data to restore the
// original state in the database.  It also drops new ticket and reversion data
// for any nodes that have a height higher than this one after.
bool WriteDisconnectedBestNode(TicketNode& node, uint256& hash, std::vector<UndoTicketData>& childUndoData){
	// Load the last best node and check to see if its height is above the
	// current node. If it is, drop all reversion data above this incoming
	// node.
	BestChainState formerBest;
	if(pdbinfoview->readBestChainState(formerBest) == false){
		return false;
	}
	if(formerBest.height > node.height){
		for(uint32_t index = formerBest.height; index > node.height; index--){
			if(pdbinfoview->earseUndoTicketData(index) == false){
				return false;
			}
			if(pdbinfoview->earseTicketHashes(index) == false){
				return false;
			}
		}
	}

	// Iterate through the block undo data and write all database
	// changes to the respective on-disk map, reversing all the
	// changes added when the child block was added to the block
	// chain.
	for(auto undo : childUndoData){
		// All flags are unset; this is a newly added ticket.
		// Remove it from the list of live tickets.
		if(!(undo.flag & TICKET_STATE_SPENT) && !(undo.flag & TICKET_STATE_REVOKED) && !(undo.flag & TICKET_STATE_MISSED)){
			if(pliveticketview->earseSingleTicketState(LiveTicketsBucketName, undo.ticketHash) == false){
				return false;
			}
		}
		// The ticket was missed and revoked.  It needs to
		// be moved from the revoked ticket treap to the
		// missed ticket treap.
		else if((undo.flag & TICKET_STATE_MISSED) && (undo.flag & TICKET_STATE_REVOKED)){
			if(prevokedticketview->earseSingleTicketState(RevokedTicketsBucketName, undo.ticketHash) == false){
				return false;
			}
			const singleTicketState sts = {
				.hash = undo.ticketHash,
				.height = undo.ticketHeight,
				.ticketstate = undo.flag
			};
			if(pmissedticketview->writeSingleTicketState(MissedTicketsBucketName, sts) == false){
				return false;
			}
		}
		// The ticket was missed and was previously live.
		// Remove it from the missed tickets bucket and
		// move it to the live tickets bucket.  We don't
		// know if it was expired or not, so just set that
		// flag to false.
		else if((undo.flag & TICKET_STATE_MISSED) && !(undo.flag & TICKET_STATE_REVOKED)){
			if(pmissedticketview->earseSingleTicketState(MissedTicketsBucketName, undo.ticketHash)){
				return false;
			}
			const singleTicketState sts = {
				.hash = undo.ticketHash,
				.height = undo.ticketHeight,
				.ticketstate = undo.flag
			};
			if(pliveticketview->writeSingleTicketState(LiveTicketsBucketName, sts)){
				return false;
			}
		}
		// The ticket was spent. Reinsert it into the live
		// tickets treap.
		else if(undo.flag & TICKET_STATE_SPENT){
			const singleTicketState sts = {
				.hash = undo.ticketHash,
				.height = undo.ticketHeight,
				.ticketstate = undo.flag
			};
			if(pliveticketview->writeSingleTicketState(LiveTicketsBucketName, sts) == false){
				return false;
			}
		}
		else{
			return error("%s: unknown ticket state in undo data", __func__);
		}
	}

	// Write the new block undo and new tickets data to the
	// database for the given height, potentially overwriting
	// an old entry with the new data.
	if(pdbinfoview->writeUndoTicketData(node.height, node.databaseUndoUpdate) == false){
		return false;
	}

	if(pdbinfoview->writeTicketHashes(node.height, node.databaseBlockTickets) == false){
		return false;
	}
	// Write the new best state to the database.
	TicketHashes nextWinners;
	nextWinners.resize(node.params.TicketsPerBlock);
	if(node.height >= uint32_t(node.params.StakeValidationHeight -1)){
		for(uint16_t num = 0; num < node.nextwinners.size(); num++){
			nextWinners[num] = node.nextwinners[num];
		}
	}
	const BestChainState bcs = {
		.hash = hash,
		.height = node.height,
		.live = (uint32_t)node.liveTickets.Len(),
		.missed = (uint64_t)node.missedTickets.Len(),
		.revoked = (uint64_t)node.revokedTickets.Len(),
		.perblock = node.params.TicketsPerBlock,
		.nextwinners = nextWinners
	};
	return pdbinfoview->writeBestChainState(bcs);
}


