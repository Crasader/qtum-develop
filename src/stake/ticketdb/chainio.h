/*
 * chainio.h
 *
 *  Created on: Dec 14, 2018
 *  Author: yaopengfei(yuitta@163.com)
 *	this file is ported from decred chainio.go
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef BITCOIN_STAKE_TICKETDB_CHAINIO_H_
#define BITCOIN_STAKE_TICKETDB_CHAINIO_H_

#include <stdio.h>
#include <uint256.h>
#include <stake/tickettreap/common.h>

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <stdint.h>
#include <memory>


// upgradeStartedBit if the bit flag for whether or not a database
// upgrade is in progress. It is used to determine if the database
// is in an inconsistent state from the update.
static const uint32_t upgradeStartedBit = 0x80000000;

// currentDatabaseVersion indicates what the current database
// version is.
static const uint32_t currentDatabaseVersion = 1;

// StakeDbInfoBucketName is the name of the database bucket used to
// house a single k->v that stores global versioning and date information for
// the stake database.
static const std::string StakeDbInfoBucketName = "stakedbinfo";

// StakeChainStateKeyName is the name of the db key used to store the best
// chain state from the perspective of the stake database.
static const std::string StakeChainStateKeyName = "stakechainstate";

// StakeBlockUndoDataBucketName is the name of the db bucket used to house the
// information used to roll back the three main databases when regressing
// backwards through the blockchain and restoring the stake information
// to that of an earlier height. It is keyed to a mainchain height.
static const std::string StakeBlockUndoDataBucketName = "stakeblockundo";

// LiveTicketsBucketName is the name of the db bucket used to house the
// list of live tickets keyed to their entry height.
static const std::string LiveTicketsBucketName = "livetickets";

// MissedTicketsBucketName is the name of the db bucket used to house the
// list of missed tickets keyed to their entry height.
static const std::string MissedTicketsBucketName = "missedtickets";

// RevokedTicketsBucketName is the name of the db bucket used to house the
// list of revoked tickets keyed to their entry height.
static const std::string RevokedTicketsBucketName = "revokedtickets";

// TicketsInBlockBucketName is the name of the db bucket used to house the
// list of tickets in a block added to the mainchain, so that it can be
// looked up later to insert new tickets into the live ticket database.
static const std::string TicketsInBlockBucketName = "ticketsinblock";

struct DatabaseInfo;
struct BestChainState;
struct UndoTicketData;
struct ticketHashes;

// TicketHashes is a list of ticket hashes that will mature in TicketMaturity
// many blocks from the block in which they were included.
using  TicketHashes = std::vector<uint256>;

// Database structure -------------------------------------------------------------
//
//   Buckets
//
// The information about the ticket database is defined by the
// StakeDbInfoBucketName bucket. By default, this bucket contains a single key
// keyed to the contents of StakeDbInfoBucketName which contains the value of
// all the database information, such as the date created and the version of
// the database.
//
// Blockchain state is stored in a root key named StakeChainStateKeyName. This
// contains the current height of the blockchain, which should be equivalent to
// the height of the best chain on start up.
//
// There are 5 buckets from the database reserved for tickets. These are:
// 1. Live
//     Live ticket bucket, for tickets currently in the lottery
//
//     k: ticket hash
//     v: height
//
// 2. Missed
//     Missed tickets bucket, for all tickets that are missed.
//
//     k: ticket hash
//     v: height
//
// 3. Revoked
//     Revoked tickets bucket, for all tickets that are Revoked.
//
//     k: ticket hash
//     v: height
//
// 4. BlockUndo
//     Block removal data, for reverting the the first 3 database buckets to
//     a previous state.
//
//     k: height
//     v: serialized undo ticket data
//
// 5. TicketsToAdd
//     Tickets to add bucket, which tells which tickets will be maturing and
//     entering the (1) in the event that a block at that height is added.
//
//     k: height
//     v: serialized list of ticket hashes
//
// For pruned nodes, both 4 and 5 can be curtailed to include only the most
// recent blocks.
//
// Procedures ---------------------------------------------------------------------
//
//   Adding a block
//
// The steps for the addition of a block are as follows:
// 1. Remove the n (constant, n=5 for all Decred networks) many tickets that were
//     selected this block.  The results of this feed into two database updates:
//         ------> A database entry containing all the data for the block
//            |     required to undo the adding of the block (as serialized
//            |     SpentTicketData and MissedTicketData)
//            \--> All missed tickets must be moved to the missed ticket bucket.
//
// 2. Expire any tickets from this block.
//     The results of this feed into two database updates:
//         ------> A database entry containing all the data for the block
//            |     required to undo the adding of the block (as serialized
//            |     MissedTicketData)
//            \--> All expired tickets must be moved to the missed ticket bucket.
//
// 3. All revocations in the block are processed, and the revoked ticket moved
//     from the missed ticket bucket to the revocations bucket:
//         ------> A database entry containing all the data for the block
//            |     required to undo the adding of the block (as serialized
//            |     MissedTicketData, revoked flag added)
//            \--> All revoked tickets must be moved to the revoked ticket bucket.
//
// 4. All newly maturing tickets must be added to the live ticket bucket. These
//     are previously stored in the "tickets to add" bucket so they can more
//     easily be pulled down when adding a block without having to load the
//     entire block itself and suffer the deserialization overhead. The only
//     things that must be written for this step are newly added tickets to the
//     ticket database, along with their respective heights.
//
//   Removing a block
//
// Steps 1 through 4 above are iterated through in reverse. The newly maturing
//  ticket hashes are fetched from the "tickets to add" bucket for the given
//  height that was used at this block height, and the tickets are dropped from
//  the live ticket bucket. The UndoTicketData is then fetched for the block and
//  iterated through in reverse order (it was stored in forward order) to restore
//  any changes to the relevant buckets made when inserting the block. Finally,
//  the data for the block removed is purged from both the BlockUndo and
//  TicketsToAdd buckets.

// -----------------------------------------------------------------------------
// The database information contains information about the version and date
// of the blockchain database.
//
//   Field      Type     Size      Description
//   version    uint32   4 bytes   The version of the database
//   date       uint32   4 bytes   The date of the creation of the database
//
// The high bit (0x80000000) is used on version to indicate that an upgrade
// is in progress and used to confirm the database fidelity on start up.
// -----------------------------------------------------------------------------

// databaseInfoSize is the serialized size of the best chain state in bytes.

// DatabaseInfo is the structure for a database.
struct DatabaseInfo{
	int64_t date;
	uint32_t version;
	bool upgradeStarted;

	template<typename Stream>
	void Serialize(Stream &s) const {
		uint32_t rawversion;
		if(upgradeStarted){
			rawversion = version | upgradeStartedBit;
			s << rawversion;
		} else {
			s << version;
		}

		s << date;
	}

	template<typename Stream>
	void Unserialize(Stream& s) {
		uint32_t rawversion;
		s >> rawversion;
		s >> date;
		version = rawversion & (~upgradeStartedBit);
		upgradeStarted = (rawversion & upgradeStartedBit) >> 31;
	}
};

// -----------------------------------------------------------------------------
// The best chain state consists of the best block hash and height, the total
// number of live tickets, the total number of missed tickets, and the number of
// revoked tickets.
//
// The serialized format is:
//
//   <block hash><block height><live><missed><revoked>
//
//   Field              Type              Size
//   block hash         chainhash.Hash    chainhash.HashSize
//   block height       uint32            4 bytes
//   live tickets       uint32            4 bytes
//   missed tickets     uint64            8 bytes
//   revoked tickets    uint64            8 bytes
//   tickets per block  uint16            2 bytes
//   next winners       []chainhash.Hash  chainhash.hashSize * tickets per block
// -----------------------------------------------------------------------------

// minimumBestChainStateSize is the minimum serialized size of the best chain
// state in bytes.
struct BestChainState{
	uint256 hash;
	int64_t height;
	uint32_t live;
	uint64_t missed;
	uint64_t revoked;
	uint16_t perblock;
	TicketHashes nextwinners;

	template<typename Stream>
	void Serialize(Stream &s) const {
		s << hash;
		s << height;
		s << live;
		s << missed;
		s << revoked;
		s << perblock;
		for(uint16_t num = 0; num < perblock; num++){
			s << nextwinners[num];
		}
	}

	template<typename Stream>
	void Unserialize(Stream& s) {
		s >> hash;
		s >> height;
		s >> live;
		s >> missed;
		s >> revoked;
		s >> perblock;
		nextwinners.resize(perblock);
		for(uint16_t num = 0; num < perblock; num++){
			s >> nextwinners[num];
		}
	}
};

// UndoTicketData is the data for any ticket that has been spent, missed, or
// revoked at some new height.  It is used to roll back the database in the
// event of reorganizations or determining if a side chain block is valid.
// The last 3 are encoded as a single byte of flags.
// The flags describe a particular state for the ticket:
//  1. Missed is set, but revoked and spent are not (0000 0001). The ticket
//      was selected in the lottery at this block height but missed, or the
//      ticket became too old and was missed. The ticket is being moved to the
//      missed ticket bucket from the live ticket bucket.
//  2. Missed and revoked are set (0000 0011). The ticket was missed
//      previously at a block before this one and was revoked, and
//      as such is being moved to the revoked ticket bucket from the
//      missed ticket bucket.
//  3. Spent is set (0000 0100). The ticket has been spent and is removed
//      from the live ticket bucket.
//  4. No flags are set. The ticket was newly added to the live ticket
//      bucket this block as a maturing ticket.
struct UndoTicketData{
	uint256 ticketHash;
	uint32_t ticketHeight;
	uint8_t flag;

	template<typename Stream>
	void Serialize(Stream &s) const {
		s << ticketHash;
		s << ticketHeight;
		s << flag;
	}

	template<typename Stream>
	void Unserialize(Stream& s) {
		s >> ticketHash;
		s >> ticketHeight;
		s >> flag;
	}

};

struct ticketHashes {
	TicketHashes hashes;

	template<typename Stream>
	void Serialize(Stream &s) const {
		for(uint16_t num = 0; num < hashes.size(); num++){
			s << hashes[num];
		}
	}

};

// single ticket state
struct singleTicketState {
	uint256 hash;
	uint32_t height;
	uint8_t ticketstate;

	template<typename Stream>
	void Serialize(Stream &s) const {
		s << hash;
		s << height;
		s << ticketstate;
	}

	template<typename Stream>
	void Unserialize(Stream& s) {
		s >> hash;
		s >> height;
		s >> ticketstate;
	}
};

#endif /* BITCOIN_STAKE_TICKETDB_CHAINIO_H_ */

