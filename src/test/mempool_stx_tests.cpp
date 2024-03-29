/*
 * mempool_stx_tests.cpp
 *
 *  Created on: Mar 27, 2019
 *  Author: yaopengfei(yuitta@163.com)
 *     this file is ported from decred tickets.go
 *     Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <policy/policy.h>
#include <txmempool.h>
#include <util.h>

#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>
#include <list>
#include <vector>

#include <wallet/test/wallet_stx_def.h>
#include <stake/staketx.h>

CMutableTransaction msstxTx;
CMutableTransaction mssgenTx;
CMutableTransaction mssrtxTx;

void mTestPreBuild() {
	///////////////////////////////////////////// sstx
	COutPoint sstxTxPre(uint256S("0x87a157f3fd88ac7907c05fc55e271dc4acdc5605d187d646604ca8c0e9382e03"), 0);
	const unsigned char sstxTxIndirect[] = {
		0x49,   // OP_DATA_73
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
	CScript sstxTxsigscript(sstxTxIndirect, sstxTxIndirect + sizeof(sstxTxIndirect));
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
	CScript sstxTxscript(sstxTxOutDirect, sstxTxOutDirect + sizeof(sstxTxOutDirect));
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
	CScript sstxTxscript1(sstxTxOutDirect1, sstxTxOutDirect1 + sizeof(sstxTxOutDirect1));
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
	CScript sstxTxscript2(sstxTxOutDirect2, sstxTxOutDirect2 + sizeof(sstxTxOutDirect2));
	CTxOut sstxTxOut2(0x2223e300, sstxTxscript2);

	const unsigned char sstxTxOutDirect3[] = {
		OP_RETURN, 30,
		0x94, 0x8c, 0x76, 0x5a, // 20 byte address
		0x69, 0x14, 0xd4, 0x3f,
		0x2a, 0x7a, 0xc1, 0x77,
		0xda, 0x2c, 0x2f, 0x6b,
		0x52, 0xde, 0x3d, 0x7c, // 7c3dde527c3dde526b2f2cda7c3dde526b2f2cda77c17a2a3fd414695a768c94
		0x00, 0xe3, 0x23, 0x21, // Transaction amount
		0x00, 0x00, 0x00, 0x80, // Last byte flagged
		0x44, 0x3f, // Fee limits
	};
	CScript sstxTxscript3(sstxTxOutDirect3, sstxTxOutDirect3 + sizeof(sstxTxOutDirect3));
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
	CScript sstxTxscript4(sstxTxOutDirect4, sstxTxOutDirect4 + sizeof(sstxTxOutDirect4));
	CTxOut sstxTxOut4(0x2223e300, sstxTxscript4);
	/////////////////////////////////////////////

	///////////////////////////////////////////// ssgen
	// ssgenTxIn0 is the 0th position input in a valid SSGen tx used to test out the
	// IsSSGen function
	COutPoint ssgenPre0(uint256(), 0xffffffff);
	const unsigned char ssgenIndirect[] = {
	   0x04, 0xff, 0xff, 0x00, 0x1d, 0x01, 0x04,
	};
	CScript ssgenscript(ssgenIndirect, ssgenIndirect + sizeof(ssgenIndirect));
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
	CScript ssgenscript1(ssgenIndirect1, ssgenIndirect1 + sizeof(ssgenIndirect1));
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
		0x52, 0xde, 0x3d, 0x7c, // 7c3dde527c3dde526b2f2cda7c3dde526b2f2cda77c17a2a3fd414695a768c94
		0x00, 0xe3, 0x23, 0x21, // 4 byte height
	};
	CScript ssgenTxscript(ssgenTxOutDirect, ssgenTxOutDirect + sizeof(ssgenTxOutDirect));
	CTxOut ssgenTxOut0(0x00000000, ssgenTxscript);

	// ssgenTxOut1 is the 1st position output in a valid SSGen tx used to test out the
	// IsSSGen function
	const unsigned char ssgenTxOutDirect1[] = {
		OP_RETURN,
		0x02,       // 2 bytes to be pushed
		0x94, 0x8c, // Vote bits
	};
	CScript ssgenTxscript1(ssgenTxOutDirect1, ssgenTxOutDirect1 + sizeof(ssgenTxOutDirect1));
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
	CScript ssgenTxscript2(ssgenTxOutDirect2, ssgenTxOutDirect2 + sizeof(ssgenTxOutDirect2));
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
	CScript ssgenTxscript3(ssgenTxOutDirect3, ssgenTxOutDirect3 + sizeof(ssgenTxOutDirect3));
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
	CScript ssrtxscript(ssrtxTxIndirect, ssrtxTxIndirect + sizeof(ssrtxTxIndirect));
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
	CScript ssrtxTxscript(ssrtxTxOutDirect, ssrtxTxOutDirect + sizeof(ssrtxTxOutDirect));
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
	CScript ssrtxTxscript1(ssrtxTxOutDirect1, ssrtxTxOutDirect1 + sizeof(ssrtxTxOutDirect1));
	CTxOut ssrtxTxOut1(0x2223e300, ssrtxTxscript1);

	msstxTx.vin.resize(3);
	msstxTx.vin[0] = sstxTxIn0;
	msstxTx.vin[1] = sstxTxIn0;
	msstxTx.vin[2] = sstxTxIn0;
	msstxTx.vout.resize(7);
	msstxTx.vout[0] = sstxTxOut0;
	msstxTx.vout[1] = sstxTxOut1;
	msstxTx.vout[2] = sstxTxOut2;
	msstxTx.vout[3] = sstxTxOut1;
	msstxTx.vout[4] = sstxTxOut2;
	msstxTx.vout[5] = sstxTxOut3;
	msstxTx.vout[6] = sstxTxOut4;
	msstxTx.nVersion = 0;
	msstxTx.nLockTime = 0;

	mssgenTx.vin.resize(2);
	mssgenTx.vin[0] = ssgenTxIn0;
	mssgenTx.vin[1] = ssgenTxIn1;
	mssgenTx.vout.resize(4);
	mssgenTx.vout[0] = ssgenTxOut0;
	mssgenTx.vout[1] = ssgenTxOut1;
	mssgenTx.vout[2] = ssgenTxOut2;
	mssgenTx.vout[3] = ssgenTxOut3;
	mssgenTx.nVersion = 0;
	mssgenTx.nLockTime = 0;

	mssrtxTx.vin.resize(1);
	mssrtxTx.vin[0] = ssrtxTxIn0;
	mssrtxTx.vout.resize(2);
	mssrtxTx.vout[0] = ssrtxTxOut0;
	mssrtxTx.vout[1] = ssrtxTxOut1;
	mssrtxTx.nVersion = 0;
	mssrtxTx.nLockTime = 0;
}

BOOST_FIXTURE_TEST_SUITE(mempool_stx_tests, TestingSetupTemp)

template<typename name>
void CheckSort(CTxMemPool &pool, std::vector<std::string> &sortedOrder)
{
	typename CTxMemPool::indexed_transaction_set::index<name>::type::iterator it = pool.mapTx.get<name>().begin();
	int count=0;
	for (; it != pool.mapTx.get<name>().end() && count < sortedOrder.size(); ++it, ++count) {
		BOOST_CHECK_EQUAL(it->GetTx().GetHash().ToString(), sortedOrder[count]);
	}
}

BOOST_AUTO_TEST_CASE(MempoolIndexingTest)
	{
	CTxMemPool pool;
	TestMemPoolEntryHelper entry;

	CMutableTransaction tx1 = CMutableTransaction();
	tx1.vout.resize(1);
	tx1.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
	tx1.vout[0].nValue = 10 * COIN;
	pool.addUnchecked(tx1.GetHash(), entry.Fee(10000LL).FromTx(tx1));

	CMutableTransaction tx2 = CMutableTransaction();
	tx2.vout.resize(1);
	tx2.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
	tx2.vout[0].nValue = 2 * COIN;
	pool.addUnchecked(tx2.GetHash(), entry.Fee(20000LL).FromTx(tx2));

	uint16_t SumNum = 100000000;
	for(uint16_t num = 0; num < SumNum; num++){
		CMutableTransaction txn = CMutableTransaction();
		txn.vout.resize(1);
		txn.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
		txn.vout[0].nValue = (int64_t)num * COIN;
		pool.addUnchecked(txn.GetHash(), entry.Fee(20000LL).FromTx(txn));
	}

	mTestPreBuild();

	CValidationStakeState stakestate;
	CMutableTransaction ssgentx0 = mssgenTx;
	ssgentx0.nVersion = 0;
	pool.addUnchecked(ssgentx0.GetHash(), entry.FromTx(ssgentx0));
	BOOST_TEST_MESSAGE(ssgentx0.GetHash().ToString());
	BOOST_CHECK(IsSSGen(CTransaction(ssgentx0), stakestate));
	stakestate.clear();

	CMutableTransaction ssgentx1 = mssgenTx;
	ssgentx1.nVersion = 1;
	pool.addUnchecked(ssgentx1.GetHash(), entry.FromTx(ssgentx1));
	BOOST_TEST_MESSAGE(ssgentx1.GetHash().ToString());
	BOOST_CHECK(IsSSGen(CTransaction(ssgentx1), stakestate));
	stakestate.clear();

	CMutableTransaction ssgentx2 = mssgenTx;
	ssgentx2.nVersion = 2;
	pool.addUnchecked(ssgentx2.GetHash(), entry.FromTx(ssgentx2));
	BOOST_TEST_MESSAGE(ssgentx2.GetHash().ToString());
	BOOST_CHECK(IsSSGen(CTransaction(ssgentx2), stakestate));

	std::vector<std::string> sortedOrder;
	sortedOrder.resize(5);
	sortedOrder[0] = ssgentx0.GetHash().ToString();
	sortedOrder[1] = ssgentx1.GetHash().ToString();
	sortedOrder[2] = ssgentx2.GetHash().ToString();
	sortedOrder[3] = tx1.GetHash().ToString();
	sortedOrder[4] = tx2.GetHash().ToString();
	LOCK(pool.cs);
	CheckSort<ssgen_sort>(pool, sortedOrder);

	}


BOOST_AUTO_TEST_SUITE_END()
