/*
 * stakedb.cpp
 *
 *  Created on: Mar 7, 2019
 *  Author: yaopengfei(yuitta@163.com)
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <stakedb.h>

std::unique_ptr<chainStateDB> pdbinfoview;
std::unique_ptr<ticketStateDB> pliveticketview;
std::unique_ptr<ticketStateDB> pmissedticketview;
std::unique_ptr<ticketStateDB> prevokedticketview;

chainStateDB::chainStateDB(size_t nCacheSize, bool fMemory, bool fWipe)
		:CDBWrapper(GetDataDir() / "stakestate", nCacheSize, fMemory, fWipe) {}	// TODO CDBWrapper true or false

ticketStateDB::ticketStateDB(std::string dbname, size_t nCacheSize, bool fMemory, bool fWipe)
		:CDBWrapper(GetDataDir() / "stakestate" / dbname, nCacheSize, fMemory, fWipe) {}	// TODO CDBWrapper true or false

// writeDatabaseInfo uses an existing database transaction to store the database
// information.
bool chainStateDB::writeDatabaseInfo(const DatabaseInfo &dbinfo) {
	CDBBatch batch(*this);
	batch.Write(StakeDbInfoBucketName, dbinfo);
	return WriteBatch(batch);
}

// readDatabaseInfo uses an existing database transaction to
// read the database versioning and creation information.
bool chainStateDB::readDatabaseInfo(DatabaseInfo &dbinfo){
	return Read(StakeDbInfoBucketName, dbinfo);
}

// writeBestChainState uses an existing database transaction to write the best chain
// state with the given parameters.
bool chainStateDB::writeBestChainState(const BestChainState &state){
	CDBBatch batch(*this);
	batch.Write(StakeChainStateKeyName, state);
	return WriteBatch(batch);
}

// readBestChainState uses an existing database transaction to get the best chain
// state.
bool chainStateDB::readBestChainState(BestChainState &state){
	return Read(StakeChainStateKeyName, state);
}

// writeUndoTicketData inserts block undo data into the database for a given height.
bool chainStateDB::writeUndoTicketData(uint32_t height, std::vector<UndoTicketData>& undoTickets){
    CDBBatch batch(*this);
	batch.Write(std::make_pair(StakeBlockUndoDataBucketName, height), undoTickets);
    return WriteBatch(batch);
}

// readUndoTicketData fetches block undo data from the database.
bool chainStateDB::readUndoTicketData(uint32_t height, std::vector<UndoTicketData>& undoTicket){
	return Read(std::make_pair(StakeBlockUndoDataBucketName, height), undoTicket);
}

// deletUndoTicketData drops block undo data from the database at a given height.
bool chainStateDB::earseUndoTicketData(uint32_t height){
	return Erase(std::make_pair(StakeBlockUndoDataBucketName, height));
}

// writeTicketHashes inserts new tickets for a mainchain block data into the
// database.
bool chainStateDB::writeTicketHashes(uint32_t height, TicketHashes &Hashes){
	CDBBatch batch(*this);
	batch.Write(std::make_pair(TicketsInBlockBucketName, height), Hashes);
	return WriteBatch(batch);
}

// readTicketHashes inserts new tickets for a mainchain block data into the
// database.
bool chainStateDB::readTicketHashes(uint32_t height, TicketHashes &Hashes){
	return ReadToVector(std::make_pair(TicketsInBlockBucketName, height), Hashes);
}

// earseTicketHashes drops new tickets for a mainchain block data at some height.
bool chainStateDB::earseTicketHashes(uint32_t height){
	return Erase(std::make_pair(TicketsInBlockBucketName, height));
}

// writeSingleTicketState inserts a ticket into one of the ticket database buckets.
bool ticketStateDB::writeSingleTicketState(std::string dbPrefix, const singleTicketState &sinTicket){
	CDBBatch batch(*this);
	batch.Write(std::make_pair(dbPrefix, sinTicket.hash), sinTicket);
	return WriteBatch(batch);
}

// DbDeleteTicket removes a ticket from one of the ticket database buckets. This
// differs from the bucket deletion method in that it will fail if the value
// itself is missing.
bool ticketStateDB::earseSingleTicketState(std::string dbPrefix, uint256& hash){
    singleTicketState sinTicket;
    if(Read(std::make_pair(dbPrefix, hash), sinTicket)){
    	error("%s: missing key %s to delete", __func__, hash.GetHex());
    }
	return Erase(std::make_pair(dbPrefix, hash));
}

// readSingleTicketState loads all the live tickets from the database into a treap.
bool ticketStateDB::readALLSingleTicketState(std::string dbPrefix, Immutable& outImu){
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

	for (pcursor->SeekToFirst(); pcursor->Valid(); pcursor->Next()){
		std::pair<std::string, uint256> key;
		if(pcursor->GetKey(key) && key.first == dbPrefix){
			singleTicketState outSingleTicket;
			if(pcursor->GetValue(outSingleTicket)){
				outImu.Put(key.second, outSingleTicket.height, outSingleTicket.ticketstate);
			} else {
				return error("%s: missing key %s to read", __func__, key.second.GetHex());
			}
		} else {
			return error("%s: the db prefix is %s, wrong key %s to read", __func__, dbPrefix, key.first);
		}
	}

	if(outImu.Len()) return true;
	return false;
}


