/*
 * ticket_tests.cpp
 *
 *  Created on: Jan 4, 2019
 *  Author: yaopengfei(yuitta@163.com)
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */


#include <test/test_bitcoin.h>
#include <stake/lottery.h>
#include <stake/tickets.h>
#include <tinyformat.h>
#include <uint256.h>
#include <utiltime.h>
#include <crypto/sha256.h>
#include <chainparams.h>
#include <chainparamsbase.h>
#include <primitives/block.h>
#include <stake/staketx.h>

#include <vector>
#include <random>
#include <map>
#include <algorithm>

#include <boost/test/unit_test.hpp>


struct TicketData{
	uint8_t Prefix;			// Bucket prefix
	uint256 SStxHash;
	uint256 SpendHash;
	int64_t BlockHeight;	// Block for where the original sstx was located
	uint32_t TxIndex;		// Position within a block, in stake tree
	bool Missed;			// Whether or not the ticket was spent
	bool Expired;			// Whether or not the ticket expired
};

// SStxMemMap is a memory map of SStx keyed to the txHash.
typedef std::map<uint256, TicketData> SStxMemMap;

BOOST_FIXTURE_TEST_SUITE(ticket_tests, BasicTestingSetup)


BOOST_AUTO_TEST_CASE(stake_lottery_num_selection){
	// Test finding ticket indexes.
	uint256 seed;
	char chr[] = "012345";
	std::vector<unsigned char> vech;
	CSHA256().Write((unsigned char*)chr, sizeof(chr)).Finalize(seed.begin());
	vech.assign(seed.begin(), seed.end());
	Hash256PRNG prng = NewHash256PRNG(vech);
	int32_t ticketsInPool = 56789;
	int32_t justEnoughTickets = 5;
	uint16_t tooFewTickets = 4;
	uint16_t ticketsPerBlock = 5;
	std::vector<int32_t> lis;

	BOOST_CHECK(!findTicketIdxs(tooFewTickets, ticketsPerBlock, prng, lis));
	lis.clear();

	BOOST_CHECK(findTicketIdxs(ticketsInPool, ticketsPerBlock, prng, lis));
	std::vector<int32_t> lisTmp = {41529, 36218, 11217, 50256, 3307};
	for(uint16_t num = 0; num < ticketsPerBlock; num++){
		BOOST_CHECK_EQUAL(lis[num], lisTmp[num]);
	}
	lis.clear();

	// Ensure that it can find all suitable ticket numbers in a small
	// bucket of tickets.
	BOOST_CHECK(findTicketIdxs(justEnoughTickets, ticketsPerBlock, prng, lis));
	lisTmp = {1, 3, 0, 2, 4};
	for(uint16_t idx = 0; idx < ticketsPerBlock; idx++){
		BOOST_CHECK_EQUAL(lis[idx], lisTmp[idx]);
	}

	uint256 lastHash;
	prng.StateHash(lastHash);
	uint256 lastHashExp = uint256S("0x8eda59e39c3c7ab8ecf2cbd020c93abf298ff0c623ad7f2364a74739f93af4cb");
	BOOST_CHECK_EQUAL(lastHash.GetHex(), lastHashExp.GetHex());
	lis.clear();

	BOOST_CHECK(!findTicketIdxs(5294967296, 5, prng, lis));
}

BOOST_AUTO_TEST_CASE(stake_lottery_fetchwinners_error){
	Immutable treap;
	for(uint16_t num = 0; num < 0xff; num++){
		uint256 key;
		std::string str = strprintf("%d", num);
		CSHA256().Write((unsigned char*)str.c_str(), str.size()).Finalize(key.begin());
		uint32_t height = num;
		uint8_t flag = 0;
		if(num % 2 == 0){
			flag |= (TICKET_STATE_MISSED | TICKET_STATE_SPENT);
		} else {
			flag |= (TICKET_STATE_REVOKED | TICKET_STATE_EXPIRED);
		}
		treap.Put(key, height, flag);
	}

	// No indexes.
	std::vector<int32_t> nilVec;
	std::vector<uint256> winners;
	BOOST_CHECK(!fetchWinners(nilVec, treap, winners));

	// No treap.
	std::vector<int32_t> vec = {1, 2, 3, 4, 5};
	Immutable niuTreap;
	BOOST_CHECK(!fetchWinners(vec, niuTreap, winners));

	// Bad index too small.
	vec[4] = -1;
	BOOST_CHECK(!fetchWinners(vec, treap, winners));

	// Bad index too big.
	vec[4] = 256;
	BOOST_CHECK(!fetchWinners(vec, treap, winners));
}

BOOST_AUTO_TEST_CASE(stake_lottery_ticket_sorting){
	uint16_t ticketsPerBlock = 5;
	uint16_t ticketPoolSize = 8192;
	uint32_t totalTickets = ticketPoolSize * 5;
	uint16_t bucketsSize = 256;

	std::minstd_rand gen(12345);
	std::vector<SStxMemMap> ticketMap;
	ticketMap.resize(bucketsSize);

	auto func = [](TicketData a, TicketData b){
		int32_t compare = a.SStxHash.GetHex().compare(b.SStxHash.GetHex());
		return compare >= 0;
	};

	uint16_t toMake = ticketPoolSize * ticketsPerBlock;
	for(uint16_t num = 0; num < toMake; num++){
		TicketData td;
		uint256 hash;
		std::string str = strprintf("%d", gen());
		CSHA256().Write((unsigned char*)str.c_str(), str.size()).Finalize(hash.begin());
		td.SStxHash = hash;
		uint8_t prefix = *hash.begin();
		ticketMap[prefix][hash] = td;
	}

	// Pre-sort with buckets (faster).
	std::vector<TicketData> sortedSlice;
	uint16_t tdnum = 0;
	sortedSlice.resize(totalTickets);
	for(uint16_t idx = 0; idx < bucketsSize; idx++){
		std::vector<TicketData> tempTdSlice;
		tempTdSlice.resize(ticketMap[idx].size());
		uint16_t i = 0;
		for(auto tdmap : ticketMap[idx]){
			tempTdSlice[i] = tdmap.second;
			i++;
		}
		std::sort(tempTdSlice.begin(), tempTdSlice.end(), func);

		for(uint16_t num = 0; num < tempTdSlice.size(); num++){
			sortedSlice[tdnum] = tempTdSlice[num];
			tdnum++;
		}
	}

	// However, it should be the same as a sort without the buckets.
	tdnum = 0;
	std::vector<TicketData> toSortSlice;
	toSortSlice.resize(totalTickets);
	for(uint16_t num = 0; num < bucketsSize; num++){
		std::vector<TicketData> tempTdSlice;
		tempTdSlice.resize(ticketMap[num].size());
		uint16_t itr = 0;
		for(auto tdmap : ticketMap[num]){
			tempTdSlice[itr] = tdmap.second;
			itr++;
		}

		for(uint16_t idx = 0; idx < tempTdSlice.size(); idx++){
			toSortSlice[tdnum] = tempTdSlice[idx];
			tdnum++;
		}
	}
	std::sort(toSortSlice.begin(), toSortSlice.end(), func);
	for(uint16_t num = 0; num < totalTickets; num++){
		if(sortedSlice[num].SStxHash.GetHex() != toSortSlice[num].SStxHash.GetHex())
			BOOST_TEST_MESSAGE(strprintf("num %d sortedSlice and toSortSlice not equal", num));
	}
}

//BOOST_AUTO_TEST_CASE(stake_tickets_ticketnode){
//
//	auto regtestChainParams = CreateChainParams(CBaseChainParams::REGTEST);
//
//	int64_t testBCHeight = 1001;
//	std::vector<CBlock> bsVec;
//	bsVec.resize(testBCHeight);
//	TicketNode bestNode;
//	bestNode.genesisNode(regtestChainParams->consensus);
//
//	std::vector<TicketNode> nodesForward;
//	nodesForward.resize(testBCHeight + 1);
//	nodesForward[0] = bestNode;
//
//	// create number of testBCHeight CBlockHeaders in bhsVec
//	uint256 lasthash = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");
//	for(int64_t num = 0; num < testBCHeight; num++){
//		bsVec[num].nVersion = 10010;
//		bsVec[num].hashPrevBlock = lasthash;
//		bsVec[num].hashMerkleRoot = uint256(strprintf("%d", num));
//		bsVec[num].nTime = (uint32_t)GetTime();
//		bsVec[num].nBits = 0xFE111111;
//		bsVec[num].nNonce = 0xFE111111;
//		char* bhstr = bsVec[num].GetHex().c_str();
//		CSHA256().Write((unsigned char*)bhstr, sizeof(bhstr)).Finalize(lasthash.begin());
//	}
//
//	// Connect to the best block (testBCHeight).
//	for(int64_t num = 1; num <= testBCHeight; num++){
//		CBlockHeader bh = bhsVec[num];
//	}
//}


BOOST_AUTO_TEST_SUITE_END()
