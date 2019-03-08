/*
 * stakedb_tests.cpp
 *
 *  Created on: Jan 2, 2019
 *  Author: yaopengfei(yuitta@163.com)
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <test/test_bitcoin.h>
#include <stakedb.h>
#include <tinyformat.h>
#include <uint256.h>
#include <utiltime.h>

#include <vector>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(stakedb_tests, TestingSetup)

// stake_ticketdb_dbinfo_serialization ensures serializing and deserializing the
// database version information works as expected.
BOOST_AUTO_TEST_CASE(stake_ticketdb_dbinfo_serialization){

	DatabaseInfo tests[] = {
		{
			.date = int64_t(0x57acca95),
			.version = currentDatabaseVersion,
			.upgradeStarted = false
		},
		{
			.date = int64_t(0x57acca95),
			.version = currentDatabaseVersion,
			.upgradeStarted = true
		}
	};
	for(uint16_t num = 0; num < 2; num++){
		pdbinfoview->writeDatabaseInfo(tests[num]);

		DatabaseInfo tempDBI;
		pdbinfoview->readDatabaseInfo(tempDBI);

		BOOST_CHECK(tempDBI.date == tests[num].date && tempDBI.version == tests[num].version && tempDBI.upgradeStarted == tests[num].upgradeStarted);
	}
}

// stake_ticketdb_bestchainstate_serialization ensures serializing and deserializing the
// best chain state works as expected.
BOOST_AUTO_TEST_CASE(stake_ticketdb_bestchainstate_serialization){

	uint16_t perblock = 5;
	TicketHashes hashes;
	for(uint16_t i = 0; i < perblock; i++){
		hashes.push_back(uint256S(strprintf("%d", i)));
	}

	BestChainState bcs = {
		.hash = uint256S("000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"),
		.height = 12323,
		.live = 29399,
		.missed = 293929392,
		.revoked = 349839493,
		.perblock = perblock,
		.nextwinners = hashes
	};
	pdbinfoview->writeBestChainState(bcs);
	BestChainState bcsOut;
	pdbinfoview->readBestChainState(bcsOut);
	BOOST_CHECK(bcs.hash.GetHex() == bcsOut.hash.GetHex() && bcs.height == bcsOut.height && bcs.live == bcsOut.live && bcs.missed == bcsOut.missed &&
			bcs.revoked == bcsOut.revoked && bcs.perblock == bcsOut.perblock);
	if(bcs.nextwinners.size() == bcsOut.nextwinners.size()){
		for(uint16_t idx = 0; idx < bcs.perblock; idx++){
			BOOST_CHECK_EQUAL(bcs.nextwinners[idx].GetHex(), bcsOut.nextwinners[idx].GetHex());
		}
	} else {
		BOOST_TEST_MESSAGE(strprintf("bcs.nextwinners and bcsOut.nextwinners vector size not equal"));
	}
}

// stake_ticketdb_undodata_serialization ensures serializing and deserializing the
// best chain state works as expected.
BOOST_AUTO_TEST_CASE(stake_ticketdb_undodata_serialization){

	std::vector<UndoTicketData> tests = {
		{
			uint256S("0x00"),
			123456,
			TICKET_STATE_MISSED | TICKET_STATE_EXPIRED
		},
		{
			uint256S("0x01"),
			122222,
			TICKET_STATE_REVOKED | TICKET_STATE_SPENT
		}
	};
	std::vector<UndoTicketData> utd;
	pdbinfoview->writeUndoTicketData(12306, tests);
	pdbinfoview->readUndoTicketData(12306, utd);
	if(tests.size() == utd.size()){
		for(uint16_t idx = 0; idx < tests.size(); idx++){
			BOOST_CHECK(tests[idx].ticketHash.GetHex() == utd[idx].ticketHash.GetHex() && tests[idx].ticketHeight == utd[idx].ticketHeight &&
					tests[idx].flag == utd[idx].flag);
		}
	}
}

BOOST_AUTO_TEST_SUITE_END()
