/*
 * stakenode.cpp
 *
 *  Created on: Jan 24, 2019
 *  Author: yaopengfei(yuitta@163.com)
 *	this file is ported from decred /blockchain/stakenode.go
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <stakenode.h>
#include <util.h>
#include <primitives/block.h>
#include <consensus/validation.h>
#include <validation.h>

#include <vector>

bool maybeFetchNewTickets(const Consensus::Params& consensusParams, CBlockIndex* node, CValidationStakeState& state){
	// Nothing to do if the tickets are already loaded.  It's important to make
	// the distinction here that nil means the value was never looked up, while
	// an empty slice means that there are no new tickets at this height.
	if(!node->newTickets.empty()){
		return true;
	}

	// No tickets in the live ticket pool are possible before stake enabled
	// height.
	if((uint64_t)node->nHeight < consensusParams.StakeEnabledHeight){
		node->newTickets.clear();
		return true;
	}

	// Calculate block number for where new tickets matured from and retrieve
	// its block from DB.
	const CBlockIndex* matureNode = node->GetAncestor(node->nHeight - (int)consensusParams.TicketMaturity);
	if(matureNode == nullptr){
		return state.Invalid(false, REJECT_INVALID, "stx-node",
				strprintf("%s: unable to obtain ancestor %d blocks prior to %s (height %d)", __func__, consensusParams.TicketMaturity, node->phashBlock->GetHex(), node->nHeight));
	}
	CBlock matureBlock;
	if(!ReadBlockFromDisk(matureBlock, matureNode, consensusParams)){
		return state.Invalid(false, REJECT_INVALID, "stx-node", strprintf("%s: ReadBlockFromDisk failed", __func__));
	}

	// Extract any ticket purchases from the block and cache them.
	std::vector<uint256> tickets;
	for(auto stx : matureBlock.svtx){
		if(IsSStx(*stx, state)){
			tickets.push_back(stx->GetHash());
		}
	}

	node->newTickets = tickets;
	return true;
}

bool maybeFetchTicketInfo(const Consensus::Params& consensusParams, CBlockIndex* node, CValidationStakeState& state){
	// Load and populate the tickets maturing in this block when they are not
	// already loaded.
	if (!maybeFetchNewTickets(consensusParams, node, state)){
		return error("%s: maybeFetchNewTickets failed. reason: %s", __func__, state.FormatStateMessage());
	}

	// Load and populate the vote and revocation information as needed.
	if(node->ticketsVoted.empty() || node->ticketsRevoked.empty() || node->votes.empty()){
		CBlock Block;
		if(!ReadBlockFromDisk(Block, node, consensusParams)){
			return state.Invalid(false, REJECT_INVALID, "stx-node", strprintf("%s: ReadBlockFromDisk failed", __func__));
		}
		SpentTicketsInBlock ticketinfo;
		FindSpentTicketsInBlock(Block, ticketinfo, state);
		node->populateTicketInfo(ticketinfo);	// TODO node safe, ypf
	}

	return true;
}

bool fetchStakeNode(const Consensus::Params& consensusParams, CBlockIndex* node, std::shared_ptr<TicketNode> stakeNode){
	TicketNode tempNode;
	CValidationStakeState state;

	// Return the cached immutable stake node when it is already loaded.
	if(!node->stakeNode->IsNull()){
		stakeNode = node->stakeNode;
		return true;
	}

	{
		LOCK(cs_main);	// TODO temp add lock here, ypf
		// Create the requested stake node from the parent stake node if it is
		// already loaded as an optimization.
		if(!node->pprev->stakeNode->IsNull()){
			// Populate the prunable ticket information as needed.
			if(!maybeFetchTicketInfo(consensusParams, node, state)){
				return error("%s: invoke maybeFetchTicketInfo failed. reason: %s", __func__, state.FormatStateMessage());
			}

			if(!connectNode(*(node->pprev->stakeNode), node->lotteryIV(), node->ticketsVoted, node->ticketsRevoked, node->newTickets, *stakeNode)){
				return state.Invalid(false, REJECT_INVALID, "stx-node", strprintf("%s: connectNode failed", __func__));
			}
			node->stakeNode = stakeNode;
			return true;
		}

		// -------------------------------------------------------------------------
		// In order to create the stake node, it is necessary to generate a path to
		// the stake node from the current tip, which always has the stake node
		// loaded, and undo the effects of each block back to, and including, the
		// fork point (which might be the requested node itself), and then, in the
		// case the target node is on a side chain, replay the effects of each on
		// the side chain.  In most cases, many of the stake nodes along the path
		// will already be loaded, so, they are only regenerated and populated if
		// they aren't.
		//
		// For example, consider the following scenario:
		//   A -> B  -> C  -> D
		//    \-> B' -> C'
		//
		// Further assume the requested stake node is for C'.  The code that follows
		// will regenerate and populate (only for those not already loaded) the
		// stake nodes for C, B, A, B', and finally, C'.
		// -------------------------------------------------------------------------

		// Start by undoing the effects from the current tip back to, and including
		// the fork point per the above description.
		CBlockIndex* tip = chainActive.Tip();
		const CBlockIndex* fork = chainActive.FindFork(static_cast<const CBlockIndex*>(node));
		for(CBlockIndex* n = tip; n != nullptr && n != fork; n = n->pprev){
			// No need to load nodes that are already loaded.
			CBlockIndex* prev = n->pprev;
			if(prev == nullptr || !prev->stakeNode->IsNull()){
				continue;
			}

			// Generate the previous stake node by starting with the child stake
			// node and undoing the modifications caused by the stake details in
			// the previous block.
			std::vector<UndoTicketData> nilUndo;
			std::vector<uint256> nilparTicket;
			if(!disconnectNode(*n->stakeNode, prev->lotteryIV(), nilUndo, nilparTicket, tempNode)){
				return state.Invalid(false, REJECT_INVALID, "stx-node", strprintf("%s: disconnectNode failed", __func__));
			}
			prev->stakeNode = std::make_shared<TicketNode>(tempNode);
		}

		// Nothing more to do if the requested node is the fork point itself.
		if(node == fork){
			stakeNode = node->stakeNode;
			return true;
		}

		// The requested node is on a side chain, so replay the effects of the
		// blocks up to the requested node per the above description.
		//
		// Note that the blocks between the fork point and the requested node are
		// added to the slice from back to front so that they are attached in the
		// appropriate order when iterating the slice.
		std::vector<CBlockIndex*> attachNodes;
		attachNodes.resize(node->nHeight - fork->nHeight);
		for(CBlockIndex* n = node; n != nullptr && n != fork; n = n->pprev){
			attachNodes[n->nHeight - fork->nHeight - 1] = n;
		}
		for(auto idx : attachNodes){
			// No need to load nodes that are already loaded.
			if(!idx->stakeNode->IsNull()){
				continue;
			}

			// Populate the prunable ticket information as needed.
			if(!maybeFetchTicketInfo(consensusParams, idx, state)){
				stakeNode->SetNull();
				return error("%s: maybeFetchTicketInfo failed. reason: %s", __func__, state.FormatStateMessage());
			}

			// Generate the stake node by applying the stake details in the current
			// block to the previous stake node.
			if(!connectNode(*(idx->pprev->stakeNode), idx->lotteryIV(), idx->ticketsVoted, idx->ticketsRevoked, idx->newTickets, tempNode)){
				return state.Invalid(false, REJECT_INVALID, "stx-node", strprintf("%s: second connectNode failed", __func__));
			}
			idx->stakeNode = std::make_shared<TicketNode>(tempNode);;
		}
	}
	stakeNode = node->stakeNode;
	return true;
}

