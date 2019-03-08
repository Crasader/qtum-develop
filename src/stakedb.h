/*
 * stakedb.h
 *
 *  Created on: Mar 7, 2019
 *  Author: yaopengfei(yuitta@163.com)
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef BITCOIN_STAKEDB_H_
#define BITCOIN_STAKEDB_H_

#include <dbwrapper.h>
#include <serialize.h>
#include <stake/ticketdb/chainio.h>

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <stdint.h>

class chainStateDB : public CDBWrapper{

public:
	explicit chainStateDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

	bool writeDatabaseInfo(const DatabaseInfo &dbinfo);
	bool readDatabaseInfo(DatabaseInfo &dbinfo);

	bool writeBestChainState(const BestChainState &state);
	bool readBestChainState(BestChainState &state);

	bool writeUndoTicketData(uint32_t height, std::vector<UndoTicketData>& undoTickets);
	bool readUndoTicketData(uint32_t height, std::vector<UndoTicketData>& undoTicket);
	bool earseUndoTicketData(uint32_t height);

    bool writeTicketHashes(uint32_t height, TicketHashes &Hashes);
    bool readTicketHashes(uint32_t height, TicketHashes &Hashes);
    bool earseTicketHashes(uint32_t height);

};

class ticketStateDB: public CDBWrapper {
public:
	explicit ticketStateDB(std::string dbname, size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool writeSingleTicketState(std::string dbPrefix, const singleTicketState &sinTicket);
    bool earseSingleTicketState(std::string dbPrefix, uint256& hash);
    bool readALLSingleTicketState(std::string dbPrefix, Immutable& outImu);
};

/** Global variable that points to dbinfo database TODO (protected by cs_main)?*/
extern std::unique_ptr<chainStateDB> pdbinfoview;

/** Global variable that points to live ticket database TODO (protected by cs_main)?*/
extern std::unique_ptr<ticketStateDB> pliveticketview;

/** Global variable that points to missed ticket database TODO (protected by cs_main)?*/
extern std::unique_ptr<ticketStateDB> pmissedticketview;

/** Global variable that points to revoked ticket database TODO (protected by cs_main)?*/
extern std::unique_ptr<ticketStateDB> prevokedticketview;


#endif /* BITCOIN_STAKEDB_H_ */
