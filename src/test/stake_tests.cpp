// Author: yaopengfei(yuitta@163.com)
// this file is ported from decred staketx.go
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/script_tests.json.h>

#include <script/script.h>
#include <script/script_error.h>
#include <script/sign.h>
#include <util.h>
#include <utilstrencodings.h>
#include <test/test_bitcoin.h>
#include <primitives/transaction.h>
#include <stake/staketx.h>
#include <stake/tickettreap/common.h>
#include <tinyformat.h>
#include <uint256.h>

#if defined(HAVE_CONSENSUS_LIB)
#include <script/bitcoinconsensus.h>
#endif

#include <fstream>
#include <stdint.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <random>
#include <unistd.h>
#include <ios>
#include <string>

#include <boost/test/unit_test.hpp>


CMutableTransaction sstxTx;
CMutableTransaction ssgenTx;
CMutableTransaction ssrtxTx;

double vm, rss;

//////////////////////////////////////////////////////////////////////////////
//
// process_mem_usage(double &, double &) - takes two doubles by reference,
// attempts to read the system-dependent data for a process' virtual memory
// size and resident set size, and return the results in KB.
//
// On failure, returns 0.0, 0.0

void process_mem_usage(double& vm_usage, double& resident_set)
{
   using std::ios_base;
   using std::ifstream;
   using std::string;

   vm_usage     = 0.0;
   resident_set = 0.0;

   // 'file' stat seems to give the most reliable results
   //
   ifstream stat_stream("/proc/self/stat",ios_base::in);

   // dummy vars for leading entries in stat that we don't care about
   //
   string pid, comm, state, ppid, pgrp, session, tty_nr;
   string tpgid, flags, minflt, cminflt, majflt, cmajflt;
   string utime, stime, cutime, cstime, priority, nice;
   string O, itrealvalue, starttime;

   // the two fields we want
   //
   unsigned long vsize;
   long rss;

   stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
               >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
               >> utime >> stime >> cutime >> cstime >> priority >> nice
               >> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest

   stat_stream.close();

   long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
   vm_usage     = vsize / 1024.0;
   resident_set = rss * page_size_kb;
}

// randHash generates a random hash using randon_device source.
void randHash(uint32_t sor, uint256& hash){
	char chr[32] = {0};
	sprintf(chr, "%ld", sor);
	hash.write(chr);
}

void TestPreBuild() {
	///////////////////////////////////////////// sstx
	COutPoint sstxTxPre(uint256S("0x87a157f3fd88ac7907c05fc55e271dc4acdc5605d187d646604ca8c0e9382e03"), 0);
	const unsigned char sstxTxIndirect[] = {
		0x49,	// OP_DATA_73
		0x30, 0x46, 0x02, 0x21, 0x00, 0xc3, 0x52, 0xd3,
		0xdd, 0x99, 0x3a, 0x98, 0x1b, 0xeb, 0xa4, 0xa6,
		0x3a, 0xd1, 0x5c, 0x20, 0x92, 0x75, 0xca, 0x94,
		0x70, 0xab, 0xfc, 0xd5, 0x7d, 0xa9, 0x3b, 0x58,
		0xe4, 0xeb, 0x5d, 0xce, 0x82, 0x02, 0x21, 0x00,
		0x84, 0x07, 0x92, 0xbc, 0x1f, 0x45, 0x60, 0x62,
		0x81, 0x9f, 0x15, 0xd3, 0x3e, 0xe7, 0x05, 0x5c,
		0xf7, 0xb5, 0xee, 0x1a, 0xf1, 0xeb, 0xcc, 0x60,
		0x28, 0xd9, 0xcd, 0xb1, 0xc3, 0xaf, 0x77, 0x48,
		0x01, // 73-byte signature
		0x65, // OP_DATA_65
		0x04, 0xf4, 0x6d, 0xb5, 0xe9, 0xd6, 0x1a, 0x9d,
		0xc2, 0x7b, 0x8d, 0x64, 0xad, 0x23, 0xe7, 0x38,
		0x3a, 0x4e, 0x6c, 0xa1, 0x64, 0x59, 0x3c, 0x25,
		0x27, 0xc0, 0x38, 0xc0, 0x85, 0x7e, 0xb6, 0x7e,
		0xe8, 0xe8, 0x25, 0xdc, 0xa6, 0x50, 0x46, 0xb8,
		0x2c, 0x93, 0x31, 0x58, 0x6c, 0x82, 0xe0, 0xfd,
		0x1f, 0x63, 0x3f, 0x25, 0xf8, 0x7c, 0x16, 0x1b,
		0xc6, 0xf8, 0xa6, 0x30, 0x12, 0x1d, 0xf2, 0xb3,
		0xd3, // 65-byte pubkey
	};
	CScript sstxTxsigscript(sstxTxIndirect, sstxTxIndirect+sizeof(sstxTxIndirect));
	CTxIn sstxTxIn0(sstxTxPre, sstxTxsigscript);

	const unsigned char sstxTxOutDirect[] = {
		OP_SSTX, OP_DUP, OP_HASH160, 20,
		0xc3, 0x98, 0xef, 0xa9,
		0xc3, 0x92, 0xba, 0x60,
		0x13, 0xc5, 0xe0, 0x4e,
		0xe7, 0x29, 0x75, 0x5e,
		0xf7, 0xf5, 0x8b, 0x32,
		OP_EQUALVERIFY, OP_CHECKSIG
	};
	CScript sstxTxscript(sstxTxOutDirect, sstxTxOutDirect+sizeof(sstxTxOutDirect));
	CTxOut sstxTxOut0(0x2123e300, sstxTxscript);

	const unsigned char sstxTxOutDirect1[] = {
		OP_RETURN, 30,
		0x94, 0x8c, 0x76, 0x5a, // 20 byte address
		0x69, 0x14, 0xd4, 0x3f,
		0x2a, 0x7a, 0xc1, 0x77,
		0xda, 0x2c, 0x2f, 0x6b,
		0x52, 0xde, 0x3d, 0x7c,
		0x00, 0xe3, 0x23, 0x21, // Transaction amount
		0x00, 0x00, 0x00, 0x00,
		0x44, 0x3f // Fee limits
	};
	CScript sstxTxscript1(sstxTxOutDirect1, sstxTxOutDirect1+sizeof(sstxTxOutDirect1));
	CTxOut sstxTxOut1(0x00000000, sstxTxscript1);

	const unsigned char sstxTxOutDirect2[] = {
		OP_SSTXCHANGE, OP_DUP, OP_HASH160, 20,
		0xc3, 0x98, 0xef, 0xa9,
		0xc3, 0x92, 0xba, 0x60,
		0x13, 0xc5, 0xe0, 0x4e,
		0xe7, 0x29, 0x75, 0x5e,
		0xf7, 0xf5, 0x8b, 0x32,
		OP_EQUALVERIFY, OP_CHECKSIG
	};
	CScript sstxTxscript2(sstxTxOutDirect2, sstxTxOutDirect2+sizeof(sstxTxOutDirect2));
	CTxOut sstxTxOut2(0x2223e300, sstxTxscript2);

	const unsigned char sstxTxOutDirect3[] = {
		OP_RETURN, 30,
		0x94, 0x8c, 0x76, 0x5a, // 20 byte address
		0x69, 0x14, 0xd4, 0x3f,
		0x2a, 0x7a, 0xc1, 0x77,
		0xda, 0x2c, 0x2f, 0x6b,
		0x52, 0xde, 0x3d, 0x7c,	// 7c3dde527c3dde526b2f2cda7c3dde526b2f2cda77c17a2a3fd414695a768c94
		0x00, 0xe3, 0x23, 0x21, // Transaction amount
		0x00, 0x00, 0x00, 0x80, // Last byte flagged
		0x44, 0x3f, // Fee limits
	};
	CScript sstxTxscript3(sstxTxOutDirect3, sstxTxOutDirect3+sizeof(sstxTxOutDirect3));
	CTxOut sstxTxOut3(0x00000000, sstxTxscript3);

	// sstxTxOut4 is the another output in the reference valid sstx, and pays change
	// to a P2SH address
	const unsigned char sstxTxOutDirect4[] = {
		OP_SSTXCHANGE, OP_HASH160,
		0x14, // OP_DATA_20
		0xc3, 0x98, 0xef, 0xa9,
		0xc3, 0x92, 0xba, 0x60,
		0x13, 0xc5, 0xe0, 0x4e,
		0xe7, 0x29, 0x75, 0x5e,
		0xf7, 0xf5, 0x8b, 0x32,
		OP_EQUAL
	};
	CScript sstxTxscript4(sstxTxOutDirect4, sstxTxOutDirect4+sizeof(sstxTxOutDirect4));
	CTxOut sstxTxOut4(0x2223e300, sstxTxscript4);
	/////////////////////////////////////////////

	///////////////////////////////////////////// ssgen
	// ssgenTxIn0 is the 0th position input in a valid SSGen tx used to test out the
	// IsSSGen function
	COutPoint ssgenPre0(uint256(), 0xffffffff);
	const unsigned char ssgenIndirect[] = {
			0x04, 0xff, 0xff, 0x00, 0x1d, 0x01, 0x04,
	};
	CScript ssgenscript(ssgenIndirect, ssgenIndirect+sizeof(ssgenIndirect));
	CTxIn ssgenTxIn0(ssgenPre0, ssgenscript);

	// ssgenTxIn1 is the 1st position input in a valid SSGen tx used to test out the
	// IsSSGen function
	COutPoint ssgenPre1(uint256S("0x87a157f3fd88ac7907c05fc55e271dc4acdc5605d187d646604ca8c0e9382e03"), 0);
	const unsigned char ssgenIndirect1[] = {
		0x49, // OP_DATA_73
		0x30, 0x46, 0x02, 0x21, 0x00, 0xc3, 0x52, 0xd3,
		0xdd, 0x99, 0x3a, 0x98, 0x1b, 0xeb, 0xa4, 0xa6,
		0x3a, 0xd1, 0x5c, 0x20, 0x92, 0x75, 0xca, 0x94,
		0x70, 0xab, 0xfc, 0xd5, 0x7d, 0xa9, 0x3b, 0x58,
		0xe4, 0xeb, 0x5d, 0xce, 0x82, 0x02, 0x21, 0x00,
		0x84, 0x07, 0x92, 0xbc, 0x1f, 0x45, 0x60, 0x62,
		0x81, 0x9f, 0x15, 0xd3, 0x3e, 0xe7, 0x05, 0x5c,
		0xf7, 0xb5, 0xee, 0x1a, 0xf1, 0xeb, 0xcc, 0x60,
		0x28, 0xd9, 0xcd, 0xb1, 0xc3, 0xaf, 0x77, 0x48,
		0x01, // 73-byte signature
		0x41, // OP_DATA_65
		0x04, 0xf4, 0x6d, 0xb5, 0xe9, 0xd6, 0x1a, 0x9d,
		0xc2, 0x7b, 0x8d, 0x64, 0xad, 0x23, 0xe7, 0x38,
		0x3a, 0x4e, 0x6c, 0xa1, 0x64, 0x59, 0x3c, 0x25,
		0x27, 0xc0, 0x38, 0xc0, 0x85, 0x7e, 0xb6, 0x7e,
		0xe8, 0xe8, 0x25, 0xdc, 0xa6, 0x50, 0x46, 0xb8,
		0x2c, 0x93, 0x31, 0x58, 0x6c, 0x82, 0xe0, 0xfd,
		0x1f, 0x63, 0x3f, 0x25, 0xf8, 0x7c, 0x16, 0x1b,
		0xc6, 0xf8, 0xa6, 0x30, 0x12, 0x1d, 0xf2, 0xb3,
		0xd3, // 65-byte pubkey
	};
	CScript ssgenscript1(ssgenIndirect1, ssgenIndirect1+sizeof(ssgenIndirect1));
	CTxIn ssgenTxIn1(ssgenPre1, ssgenscript1);

	// ssgenTxOut0 is the 1st position output in a valid SSGen tx used to test out the
	// IsSSGen function
	const unsigned char ssgenTxOutDirect[] = {
		OP_RETURN,
		0x24,                   // 36 bytes to be pushed
		0x94, 0x8c, 0x76, 0x5a, // 32 byte hash
		0x69, 0x14, 0xd4, 0x3f,
		0x2a, 0x7a, 0xc1, 0x77,
		0xda, 0x2c, 0x2f, 0x6b,
		0x52, 0xde, 0x3d, 0x7c,
		0xda, 0x2c, 0x2f, 0x6b,
		0x52, 0xde, 0x3d, 0x7c,
		0x52, 0xde, 0x3d, 0x7c,	// 7c3dde527c3dde526b2f2cda7c3dde526b2f2cda77c17a2a3fd414695a768c94
		0x00, 0xe3, 0x23, 0x21, // 4 byte height
	};
	CScript ssgenTxscript(ssgenTxOutDirect, ssgenTxOutDirect+sizeof(ssgenTxOutDirect));
	CTxOut ssgenTxOut0(0x00000000, ssgenTxscript);

	// ssgenTxOut1 is the 1st position output in a valid SSGen tx used to test out the
	// IsSSGen function
	const unsigned char ssgenTxOutDirect1[] = {
		OP_RETURN,
		0x02,       // 2 bytes to be pushed
		0x94, 0x8c, // Vote bits
	};
	CScript ssgenTxscript1(ssgenTxOutDirect1, ssgenTxOutDirect1+sizeof(ssgenTxOutDirect1));
	CTxOut ssgenTxOut1(0x00000000, ssgenTxscript1);

	// ssgenTxOut2 is the 2nd position output in a valid SSGen tx used to test out the
	// IsSSGen function
	const unsigned char ssgenTxOutDirect2[] = {
		OP_SSGEN, OP_DUP, OP_HASH160,
		0x14, // OP_DATA_20
		0xc3, 0x98, 0xef, 0xa9,
		0xc3, 0x92, 0xba, 0x60,
		0x13, 0xc5, 0xe0, 0x4e,
		0xe7, 0x29, 0x75, 0x5e,
		0xf7, 0xf5, 0x8b, 0x32,
		OP_EQUALVERIFY, OP_CHECKSIG
	};
	CScript ssgenTxscript2(ssgenTxOutDirect2, ssgenTxOutDirect2+sizeof(ssgenTxOutDirect2));
	CTxOut ssgenTxOut2(0x00000000, ssgenTxscript2);

	// ssgenTxOut3 is a P2SH output
	const unsigned char ssgenTxOutDirect3[] = {
		OP_SSGEN, OP_HASH160,
		0x14, // OP_DATA_20
		0xc3, 0x98, 0xef, 0xa9,
		0xc3, 0x92, 0xba, 0x60,
		0x13, 0xc5, 0xe0, 0x4e,
		0xe7, 0x29, 0x75, 0x5e,
		0xf7, 0xf5, 0x8b, 0x32,
		OP_EQUAL
	};
	CScript ssgenTxscript3(ssgenTxOutDirect3, ssgenTxOutDirect3+sizeof(ssgenTxOutDirect3));
	CTxOut ssgenTxOut3(0x2123e300, ssgenTxscript3);

	/////////////////////////////////////////////

	///////////////////////////////////////////// ssrtx
	// ssrtxTxIn is the 0th position input in a valid SSRtx tx used to test out the
	// IsSSRtx function
	COutPoint ssrtxTxPre(uint256S("0x87a157f3fd88ac7907c05fc55e271dc4acdc5605d187d646604ca8c0e9382e03"), 0);
	const unsigned char ssrtxTxIndirect[] = {
			0x49, // OP_DATA_73
			0x30, 0x46, 0x02, 0x21, 0x00, 0xc3, 0x52, 0xd3,
			0xdd, 0x99, 0x3a, 0x98, 0x1b, 0xeb, 0xa4, 0xa6,
			0x3a, 0xd1, 0x5c, 0x20, 0x92, 0x75, 0xca, 0x94,
			0x70, 0xab, 0xfc, 0xd5, 0x7d, 0xa9, 0x3b, 0x58,
			0xe4, 0xeb, 0x5d, 0xce, 0x82, 0x02, 0x21, 0x00,
			0x84, 0x07, 0x92, 0xbc, 0x1f, 0x45, 0x60, 0x62,
			0x81, 0x9f, 0x15, 0xd3, 0x3e, 0xe7, 0x05, 0x5c,
			0xf7, 0xb5, 0xee, 0x1a, 0xf1, 0xeb, 0xcc, 0x60,
			0x28, 0xd9, 0xcd, 0xb1, 0xc3, 0xaf, 0x77, 0x48,
			0x01, // 73-byte signature
			0x41, // OP_DATA_65
			0x04, 0xf4, 0x6d, 0xb5, 0xe9, 0xd6, 0x1a, 0x9d,
			0xc2, 0x7b, 0x8d, 0x64, 0xad, 0x23, 0xe7, 0x38,
			0x3a, 0x4e, 0x6c, 0xa1, 0x64, 0x59, 0x3c, 0x25,
			0x27, 0xc0, 0x38, 0xc0, 0x85, 0x7e, 0xb6, 0x7e,
			0xe8, 0xe8, 0x25, 0xdc, 0xa6, 0x50, 0x46, 0xb8,
			0x2c, 0x93, 0x31, 0x58, 0x6c, 0x82, 0xe0, 0xfd,
			0x1f, 0x63, 0x3f, 0x25, 0xf8, 0x7c, 0x16, 0x1b,
			0xc6, 0xf8, 0xa6, 0x30, 0x12, 0x1d, 0xf2, 0xb3,
			0xd3, // 65-byte pubkey
	};
	CScript ssrtxscript(ssrtxTxIndirect, ssrtxTxIndirect+sizeof(ssrtxTxIndirect));
	CTxIn ssrtxTxIn0(ssrtxTxPre, ssrtxscript);

	// ssrtxTxOut is the 0th position output in a valid SSRtx tx used to test out the
	// IsSSRtx function
	const unsigned char ssrtxTxOutDirect[] = {
		OP_SSRTX, OP_DUP, OP_HASH160, 20,
		0xc3, 0x98, 0xef, 0xa9,
		0xc3, 0x92, 0xba, 0x60,
		0x13, 0xc5, 0xe0, 0x4e,
		0xe7, 0x29, 0x75, 0x5e,
		0xf7, 0xf5, 0x8b, 0x32,
		OP_EQUALVERIFY, OP_CHECKSIG
	};
	CScript ssrtxTxscript(ssrtxTxOutDirect, ssrtxTxOutDirect+sizeof(ssrtxTxOutDirect));
	CTxOut ssrtxTxOut0(0x2123e300, ssrtxTxscript);

	const unsigned char ssrtxTxOutDirect1[] = {
		OP_SSRTX, OP_HASH160,
		0x14, // OP_DATA_20
		0xc3, 0x98, 0xef, 0xa9,
		0xc3, 0x92, 0xba, 0x60,
		0x13, 0xc5, 0xe0, 0x4e,
		0xe7, 0x29, 0x75, 0x5e,
		0xf7, 0xf5, 0x8b, 0x32,
		OP_EQUAL
	};
	CScript ssrtxTxscript1(ssrtxTxOutDirect1, ssrtxTxOutDirect1+sizeof(ssrtxTxOutDirect1));
	CTxOut ssrtxTxOut1(0x2223e300, ssrtxTxscript1);

	sstxTx.vin.resize(3);
	sstxTx.vin[0] = sstxTxIn0;
	sstxTx.vin[1] = sstxTxIn0;
	sstxTx.vin[2] = sstxTxIn0;
	sstxTx.vout.resize(7);
	sstxTx.vout[0] = sstxTxOut0;
	sstxTx.vout[1] = sstxTxOut1;
	sstxTx.vout[2] = sstxTxOut2;
	sstxTx.vout[3] = sstxTxOut1;
	sstxTx.vout[4] = sstxTxOut2;
	sstxTx.vout[5] = sstxTxOut3;
	sstxTx.vout[6] = sstxTxOut4;
	sstxTx.nVersion = 0;
	sstxTx.nLockTime = 0;

	ssgenTx.vin.resize(2);
	ssgenTx.vin[0] = ssgenTxIn0;
	ssgenTx.vin[1] = ssgenTxIn1;
	ssgenTx.vout.resize(4);
	ssgenTx.vout[0] = ssgenTxOut0;
	ssgenTx.vout[1] = ssgenTxOut1;
	ssgenTx.vout[2] = ssgenTxOut2;
	ssgenTx.vout[3] = ssgenTxOut3;
	ssgenTx.nVersion = 0;
	ssgenTx.nLockTime = 0;

	ssrtxTx.vin.resize(1);
	ssrtxTx.vin[0] = ssrtxTxIn0;
	ssrtxTx.vout.resize(2);
	ssrtxTx.vout[0] = ssrtxTxOut0;
	ssrtxTx.vout[1] = ssrtxTxOut1;
	ssrtxTx.nVersion = 0;
	ssrtxTx.nLockTime = 0;
}

BOOST_FIXTURE_TEST_SUITE(stake_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(ssgen_validity){
	TestPreBuild();
	CValidationStakeState state;
	BOOST_CHECK(IsSSGen(ssgenTx, state));

	state.clear();
	BOOST_CHECK(IsSSRtx(ssrtxTx, state));
}

BOOST_AUTO_TEST_CASE(ssgen_util){
	uint256 hash;
	uint32_t height;
	SSGenBlockVotedOn(ssgenTx, hash, height);

	BOOST_CHECK_EQUAL(hash.GetHex(), "7c3dde527c3dde526b2f2cda7c3dde526b2f2cda77c17a2a3fd414695a768c94");
	BOOST_CHECK_EQUAL(height, 556000000);
	BOOST_CHECK_EQUAL(SSGenVoteBits(ssgenTx), 35988);

	vec64 amounts{
		21000000,
		11000000,
		10000000,
	};
	int64_t amountTicket = 42000000;
	int64_t subsidy = 400000;

	vec64 outAmts = CalculateRewards(amounts, amountTicket, subsidy);
	vec64 expectedAmts{
		21200000,
		11104761,
		10095238,
	};
	for(uint32_t num = 0; num < outAmts.size(); num++){
		BOOST_CHECK_EQUAL(outAmts[num], expectedAmts[num]);
	}
}

BOOST_AUTO_TEST_CASE(sstx_util){
	CValidationStakeState state;
	BOOST_CHECK(IsSStx(sstxTx, state));

	vec64 commitAmts{
		0x2122e300,
		0x12000000,
		0x12300000,
	};

	vec64 changeAmts{
		0x0122e300,
		0x02000000,
		0x02300000,
	};

	int64_t amtTicket = 0x9122e300;

	int64_t fees = 0;
	vec64 contribAmounts;
	state.clear();
	BOOST_CHECK(SStxNullOutputAmounts(commitAmts, changeAmts, amtTicket, fees, contribAmounts, state));
	vec64 expectedAmts{
		0x20000000,
		0x10000000,
		0x10000000,
	};

	for(uint32_t num = 0; num < contribAmounts.size(); num++){
		BOOST_CHECK_EQUAL(expectedAmts[num], contribAmounts[num]);
	}
}

BOOST_AUTO_TEST_CASE(ticket_treap_common){
	int16_t tests[] = {1, staticDepth, staticDepth + 1};
	bool back = false;

	for(uint16_t u = 0; u < 3; u++){
		std::vector<treapNode*> vecTreap;
		vecTreap.resize((uint64_t)tests[u]);
		for(uint16_t idx = 0; idx < tests[u]; idx++){
			uint256 key = uint256S(strprintf("%d", idx));
			uint32_t height = (uint32_t)idx;
			treapNode node(key, height, 0, height);
			vecTreap[idx] = &node;
		}

		// Push all of the nodes onto the parent stack while testing
		// various stack properties.
		parentStack stack;
		for(uint16_t idx = 0; idx < tests[u]; idx++){
			stack.Push(vecTreap[idx]);

			// Ensure the stack length is the expected value.
			if(stack.Len() != idx +1){
				BOOST_TEST_MESSAGE(strprintf("Len #%d (%d): unexpected stack length - got %d, want %d"
						, u, idx, stack.Len(), idx+1));
				back = true;
				break;
			}

			// Ensure the node at each index is the expected one.
			for(uint16_t num = 0; num <= idx; num++){
				treapNode* atNode = stack.At(idx - num);
				if(!(atNode == vecTreap[num])){
					BOOST_TEST_MESSAGE(strprintf("At #%d (%d): mismatched node", u, idx-num));
					back = true;
					break;
				}
			}
			if(back == true){
				break;
			}
		}
		if(back == true){
			back = false;
			continue;
		}

		// Ensure each popped node is the expected one.
		for(uint16_t idx = 0; idx < tests[u]; idx++){
			treapNode* node = stack.Pop();
			uint16_t len = tests[u] - idx -1;
			if(node != vecTreap[len]){
				BOOST_TEST_MESSAGE(strprintf("At #%d (%d): mismatched node", u, idx));
				back = true;
				break;
			}
		}
		if(back == true){
			back = false;
			continue;
		}

		// Ensure the stack is now empty.
		if(stack.Len() != 0){
			BOOST_TEST_MESSAGE(strprintf("Len #%d: stack is not empty - got %d", u, stack.Len()));
			continue;
		}

		// Ensure attempting to retrieve a node at an index beyond the
		// stack's length returns nil.
		treapNode* nodeOut = stack.At(2);
		if(nodeOut != nullptr){
			BOOST_TEST_MESSAGE(strprintf("At #%d: did not give back nil ", u));
			continue;
		}

		// Ensure attempting to pop a node from an empty stack returns
		// nil.
		treapNode* nodeOut2 = stack.Pop();
		if(nodeOut2 != nullptr){
			BOOST_TEST_MESSAGE(strprintf("Pop #%d: did not give back nil", u));
			continue;
		}
	}
}

// ticket_treap_immutable_empty ensures calling functions on an empty immutable treap
// works as expected.
BOOST_AUTO_TEST_CASE(ticket_treap_immutable_empty){
	// Ensure the treap length is the expected value.
	Immutable testTreap;
	int64_t gotLen = testTreap.Len();
	if(gotLen != 0){
		BOOST_TEST_MESSAGE(strprintf("Len: unexpected length - got %d, want 0", gotLen));
	}

	// Ensure the reported size is 0.
	uint64_t gotSize = testTreap.Size();
	if(gotSize != 0){
		BOOST_TEST_MESSAGE(strprintf("Size: unexpected byte size - got %d, want 0",	gotSize));
	}

	// Ensure there are no errors with requesting keys from an empty treap.
	uint256 key = uint256S("0");
	if(testTreap.Has(key)){
		BOOST_TEST_MESSAGE("Has: unexpected result");
	}

	// Ensure there are no panics when deleting keys from an empty treap.
	testTreap.Delete(key);

	// Ensure the number of keys iterated by ForEach on an empty treap is
	// zero.
	TicketHashes hashes;
	testTreap.ForEach(hashes);
	BOOST_CHECK_EQUAL(hashes.size(), 0);

//	uint256 keyOut;
//	uint32_t height;
//	uint8_t flag;
//	testTreap.GetByIndex(-1, keyOut, height, flag);

//	testTreap.GetByIndex(0, keyOut, height, flag);
}

// ticket_treap_immutable_sequential ensures that putting keys into an immutable treap in
// sequential order works as expected.
BOOST_AUTO_TEST_CASE(ticket_treap_immutable_sequential){

	// Insert a bunch of sequential keys while checking several of the treap
	// functions work as expected.
	uint16_t numItems = 20;
	Immutable testTreap;
	for(uint16_t i = 0; i < numItems; i++){
		uint256 key = uint256S(strprintf("%d", i));
		uint32_t height = (uint32_t)i;
		if(height == 0){
			BOOST_CHECK(!testTreap.Put(key, height, 0));
		} else {
			BOOST_CHECK(testTreap.Put(key, height, 0));
		}

		// Ensure the treap length is the expected value.
		BOOST_CHECK_EQUAL(testTreap.Len(), i + 1);

		// Ensure the treap has the key.
		BOOST_CHECK(testTreap.Has(key));

		treapNode* nodeOut = testTreap.Get(key);
		if(nodeOut != nullptr)
			BOOST_CHECK_EQUAL(nodeOut->mheight, height);

		// test GetByIndex
		uint256 keyOut;
		uint32_t heightOut;
		uint8_t flagOut;
		testTreap.GetByIndex(i, keyOut, heightOut, flagOut);

		BOOST_CHECK_EQUAL(key.GetHex(), keyOut.GetHex());
		BOOST_CHECK_EQUAL(height, heightOut);
	}

	BOOST_CHECK(testTreap.testHeap());

	// Delete the keys one-by-one while checking several of the treap
	// functions work as expected.
	for(uint16_t i = 0; i < numItems; i++){
		uint256 key = uint256S(strprintf("%d", i));
		testTreap.Delete(key);

		uint16_t expectedLen = numItems - i - 1;
		uint16_t expectedHeadValue = i + 1;

		// Ensure the treap length is the expected value.
		int64_t gotLen = testTreap.Len();
		BOOST_CHECK_EQUAL(gotLen, expectedLen);

		BOOST_CHECK(!testTreap.Has(key));

		// test GetByIndex
		uint256 keyOut;
		uint32_t heightOut;
		uint8_t flagOut;

		if(expectedLen > 0){

			testTreap.GetByIndex(0, keyOut, heightOut, flagOut);
			BOOST_CHECK_EQUAL(std::strtoll(keyOut.GetHex().c_str(), nullptr, 10), expectedHeadValue);

			uint16_t halfIdx = expectedLen / 2;
			testTreap.GetByIndex(halfIdx, keyOut, heightOut, flagOut);
			BOOST_CHECK_EQUAL(std::strtoll(keyOut.GetHex().c_str(), nullptr, 10), expectedHeadValue + halfIdx);

			testTreap.GetByIndex(expectedLen - 1, keyOut, heightOut, flagOut);
			BOOST_CHECK_EQUAL(std::strtoll(keyOut.GetHex().c_str(), nullptr, 10), expectedHeadValue + expectedLen - 1);
		}

		// Get the key that no longer exists from the treap and ensure
		// it is nil.
		BOOST_CHECK(!testTreap.Get(key));

	}
}

// TestImmutableReverseSequential ensures that putting keys into an immutable
// treap in reverse sequential order works as expected.
BOOST_AUTO_TEST_CASE(ticket_treap_immutable_reverse_sequential){
	// Insert a bunch of sequential keys while checking several of the treap
	// functions work as expected.
	uint16_t numItems = 20;
	Immutable testTreap;
	for(uint16_t i = 0; i < numItems; i++){
		uint256 key = uint256S(strprintf("%d", numItems - i - 1));
		uint32_t height = (uint32_t)(numItems - i - 1);
		if(height == (uint32_t)(numItems - 1)){
			BOOST_CHECK(!testTreap.Put(key, height, 0));
		} else {
			BOOST_CHECK(testTreap.Put(key, height, 0));
		}

		// Ensure the treap length is the expected value.
		BOOST_CHECK_EQUAL(testTreap.Len(), i + 1);

		// Ensure the treap has the key.
		BOOST_CHECK(testTreap.Has(key));

		treapNode* nodeOut = testTreap.Get(key);
		if(nodeOut != nullptr)
			BOOST_CHECK_EQUAL(nodeOut->mheight, height);

	}

	BOOST_CHECK(testTreap.testHeap());

	// Delete the keys one-by-one while checking several of the treap
	// functions work as expected.
	for(uint16_t i = 0; i < numItems; i++){
		uint256 key = uint256S(strprintf("%d", i));
		testTreap.Delete(key);

		// Ensure the treap length is the expected value.
		int64_t gotLen = testTreap.Len();
		BOOST_CHECK_EQUAL(gotLen, numItems - i - 1);

		BOOST_CHECK(!testTreap.Has(key));

		// Get the key that no longer exists from the treap and ensure
		// it is nil.
		BOOST_CHECK(!testTreap.Get(key));

		BOOST_CHECK(testTreap.testHeap());

	}
}

// TestImmutableUnordered ensures that putting keys into an immutable treap in
// no paritcular order works as expected.
BOOST_AUTO_TEST_CASE(ticket_treap_immutable_unordered){
	// Insert a bunch of out-of-order keys while checking several of the
	// treap functions work as expected.
	uint16_t numItems = 20;
	Immutable testTreap;
	for(uint16_t i = 0; i < numItems; i++){
		uint256 key;
		std::string str = strprintf("%d", i);
		CSHA256().Write((unsigned char*)str.c_str(), str.size()).Finalize(key.begin());
		uint32_t height = (uint32_t)(i);
		if(height == 0){
			BOOST_CHECK(!testTreap.Put(key, height, 0));
		} else {
			BOOST_CHECK(testTreap.Put(key, height, 0));
		}

		// Ensure the treap length is the expected value.
		BOOST_CHECK_EQUAL(testTreap.Len(), i + 1);

		// Ensure the treap has the key.
		BOOST_CHECK(testTreap.Has(key));

		treapNode* nodeOut = testTreap.Get(key);
		if(nodeOut != nullptr)
			BOOST_CHECK_EQUAL(nodeOut->mheight, height);

	}

	BOOST_CHECK(testTreap.testHeap());

	// Delete the keys one-by-one while checking several of the treap
	// functions work as expected.
	for(uint16_t i = 0; i < numItems; i++){
		uint256 key;
		CSHA256().Write((unsigned char*)strprintf("%d", i).c_str(), strprintf("%d", i).size()).Finalize(key.begin());
		testTreap.Delete(key);

		// Ensure the treap length is the expected value.
		int64_t gotLen = testTreap.Len();
		BOOST_CHECK_EQUAL(gotLen, numItems - i - 1);

		BOOST_CHECK(!testTreap.Has(key));

		// Get the key that no longer exists from the treap and ensure
		// it is nil.
		BOOST_CHECK(!testTreap.Get(key));

	}

}

// ticket_treap_immutable_duplicateput ensures that putting a duplicate key into an
// immutable treap works as expected.
BOOST_AUTO_TEST_CASE(ticket_treap_immutable_duplicateput){
	uint32_t expectedH = 10000;
	uint16_t numItems = 20;
	Immutable testTreap;

	for(uint16_t i = 0; i < numItems; i++){
		uint256 key = uint256S(strprintf("%d", i));
		uint32_t height = (uint32_t)i;
		testTreap.Put(key, height, 0);

		// Put a duplicate key with the the expected final value.
		testTreap.Put(key, expectedH, 0);

		// Ensure the key still exists and is the new value.
		BOOST_CHECK(testTreap.Has(key));

		treapNode* nodeOut = testTreap.Get(key);
		if(nodeOut != nullptr)
			BOOST_CHECK_EQUAL(nodeOut->mheight, expectedH);
	}
}

// ticket_treap_immutable_nilvalue ensures that putting a nil value into an immutable
// treap results in a NOOP.
BOOST_AUTO_TEST_CASE(ticket_treap_immutable_nilvalue){
	uint256 key = uint256S(strprintf("%d", 0));
	Immutable testTreap;
	// Put the key with a nil value.
	testTreap.Put(key, 0, 0);

	// Ensure the key does NOT exist.
	BOOST_CHECK(!testTreap.Has(key));

	BOOST_CHECK(testTreap.Get(key) == nullptr);
}

// TestImmutableSnapshot ensures that immutable treaps are actually immutable by
// keeping a reference to the previous treap, performing a mutation, and then
// ensuring the referenced treap does not have the mutation applied.
BOOST_AUTO_TEST_CASE(ticket_treap_immutable_snapshot){


	// Insert a bunch of sequential keys while checking several of the treap
	// functions work as expected.
	uint16_t numItems = 20;
	Immutable testTreap;
	for(uint16_t i = 0; i < numItems; i++){
		uint256 key = uint256S(strprintf("%d", i));
		uint32_t height = (uint32_t)i;
		if(height == 0){
			BOOST_CHECK(!testTreap.Put(key, height, 0));
		} else {
			BOOST_CHECK(testTreap.Put(key, height, 0));
		}

		// Ensure the treap length is the expected value.
		BOOST_CHECK_EQUAL(testTreap.Len(), i + 1);

		// Ensure the treap has the key.
		BOOST_CHECK(testTreap.Has(key));

		treapNode* nodeOut = testTreap.Get(key);
		if(nodeOut != nullptr)
			BOOST_CHECK_EQUAL(nodeOut->mheight, height);
	}

	// Delete the keys one-by-one while checking several of the treap
	// functions work as expected.
	Immutable* treapSnap = &testTreap;
	for(uint16_t i = 0; i < numItems; i++){
		uint256 key = uint256S(strprintf("%d", i));
		testTreap.Delete(key);

		// Ensure the treap length is the expected value.
		int64_t gotLen = treapSnap->Len();
		BOOST_CHECK_EQUAL(gotLen, numItems - i - 1);

		BOOST_CHECK(!treapSnap->Has(key));

		// Get the key that no longer exists from the treap and ensure
		// it is nil.
		BOOST_CHECK(!treapSnap->Get(key));

	}
}

// ticket_treap_immutable_memory tests the memory for creating n many nodes cloned and
// modified in the memory analogous to what is actually seen in the Decred
// mainnet, then analyzes the relative memory usage with runtime stats.
BOOST_AUTO_TEST_CASE(ticket_treap_immutable_memory){
	process_mem_usage(vm, rss);
	double initAlloc = rss;
	vm = rss = 0;

	std::random_device rd;
	uint16_t numItems = 40960;
	uint16_t numNodes = 128;
	Immutable testTreap;
	std::vector<Immutable> nodeTreaps;
	nodeTreaps.resize(numNodes);

	// Populate
	for(uint16_t num = 0; num < numItems; num++){
		uint256 hash;
		randHash(rd(), hash);
		testTreap.Put(hash, (uint32_t)rd(), TICKET_STATE_REVOKED | TICKET_STATE_EXPIRED);
	}
	nodeTreaps[0] = testTreap;

	// Start populating the "nodes". Ignore expiring tickets for the
	// sake of testing. For each node, remove 5 "random" tickets and
	// insert 5 "random" tickets.
	process_mem_usage(vm, rss);
	double lastTotal = rss;
	vm = rss = 0;
	uint32_t maxHeight = UINT_MAX;
	Immutable lastTreap(nodeTreaps[0]);
	uint32_t allocsPerNode[numNodes] = {0};
	for(uint16_t num = 0; num < numNodes; num++){
		std::vector<uint32_t> winnerIdx;
		winnerIdx.resize(5);
		int64_t sz = lastTreap.Len();
		std::vector<uint256*> winners;
		std::vector<uint256*> expires;
		for(uint16_t idx = 0; idx < 5; idx++){
			winnerIdx[idx] = rd() % (uint32_t)sz;
		}
		lastTreap.FetchWinnersAndExpired(winnerIdx, maxHeight, winners, expires);
		for(uint256* phash : winners){
			lastTreap.Delete(*phash);
		}

		for(uint16_t i = 0; i < 5; i++){
			uint256 hash;
			randHash(rd(), hash);
			lastTreap.Put(hash, (uint32_t)rd(), TICKET_STATE_REVOKED | TICKET_STATE_EXPIRED);
		}
		process_mem_usage(vm, rss);
		double finalTotal = rss;
		vm = rss = 0;
		allocsPerNode[num] = finalTotal - lastTotal;
		lastTotal = finalTotal;
		nodeTreaps[num] = lastTreap;
	}
	uint64_t sum = 0;
	uint64_t avg = 0;
	for(uint16_t j = 0; j < numNodes; j++){
		sum += allocsPerNode[j];
	}
	avg = sum / numNodes;

	process_mem_usage(vm, rss);
	double finalAlloc = rss;
	vm = rss = 0;

	BOOST_TEST_MESSAGE(strprintf("Ticket treaps for %d nodes allocated %ld many bytes total after GC", numNodes, finalAlloc-initAlloc));

	BOOST_TEST_MESSAGE(strprintf("Ticket treaps allocated an average of %d many bytes per node", avg));
}

BOOST_AUTO_TEST_SUITE_END()
