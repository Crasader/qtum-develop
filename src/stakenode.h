/*
 * stakenode.h
 *
 *  Created on: Jan 24, 2019
 *  Author: yaopengfei(yuitta@163.com)
 *	this file is ported from decred /blockchain/stakenode.go
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef BITCOIN_STAKENODE_H_
#define BITCOIN_STAKENODE_H_

#include <consensus/params.h>
#include <wallet/wallet.h>
#include <stake/staketx.h>
#include <stake/tickets.h>

// maybeFetchNewTickets loads the list of newly maturing tickets for a given
// node by traversing backwards through its parents until it finds the block
// that contains the original tickets to mature if needed.
//
// This function MUST be called with the chain state lock held (for writes).
bool maybeFetchNewTickets(const Consensus::Params& consensusParams, CBlockIndex* node, CValidationStakeState& state);

// maybeFetchTicketInfo loads and populates prunable ticket information in the
// provided block node if needed.
//
// This function MUST be called with the chain state lock held (for writes).
bool maybeFetchTicketInfo(const Consensus::Params& consensusParams, CBlockIndex* node, CValidationStakeState& state);

// fetchStakeNode returns the stake node associated with the requested node
// while handling the logic to create the stake node if needed.  In the majority
// of cases, the stake node either already exists and is simply returned, or it
// can be quickly created when the parent stake node is already available.
// However, it should be noted that, since old stake nodes are pruned, this
// function can be quite expensive if a node deep in history or on a long side
// chain is requested since that requires reconstructing all of the intermediate
// nodes along the path from the existing tip to the requested node that have
// not already been pruned.
//
// This function MUST be called with the chain state lock held (for writes).
bool fetchStakeNode(const Consensus::Params& consensusParams, CBlockIndex* node, std::shared_ptr<TicketNode> stakeNode);



#endif /* BITCOIN_STAKENODE_H_ */
