/*
 * wallet_stx_tests.cpp
 *
 *  Created on: Feb 16, 2019
 *  Author: yaopengfei(yuitta@163.com)
 *  Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <wallet/wallet.h>

#include <set>
#include <stdint.h>
#include <utility>
#include <vector>

#include <consensus/validation.h>
#include <rpc/server.h>
#include <test/test_bitcoin.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/test/wallet_test_fixture.h>
#include <wallet/test/wallet_stx_def.h>
#include <chainparams.h>
#include <pos.h>
#include <stakenode.h>

#include <boost/test/unit_test.hpp>
#include <univalue.h>

#include <timedata.h>
#include <random.h>

static void AddKey(CWallet& wallet, const CKey& key)
{
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

class ListCoinsTestingSetup : public TestChain100SetupTmp
{
public:
    ListCoinsTestingSetup()
    {
        CreateAndProcessBlock({}, {}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        ::bitdb.MakeMock();
        g_address_type = OUTPUT_TYPE_DEFAULT;
        g_change_type = OUTPUT_TYPE_DEFAULT;
        wallet.reset(new CWallet(std::unique_ptr<CWalletDBWrapper>(new CWalletDBWrapper(&bitdb, "wallet_test.dat"))));
        bool firstRun;
        wallet->LoadWallet(firstRun);
        RegisterValidationInterface(wallet.get());

        AddKey(*wallet, coinbaseKey);
        WalletRescanReserver reserver(wallet.get());
        reserver.reserve();
        wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver);
        wallet->SetBroadcastTransactions(true);
    }

    ~ListCoinsTestingSetup()
    {
    	UnregisterValidationInterface(wallet.get());
        wallet.reset();
        ::bitdb.Flush(true);
        ::bitdb.Reset();
    }

    void AddTx2(CRecipient recipient)	// modify produce 5 transactions with one utxo
    {
    	std::vector<CWalletTx> wtxs;
        for(uint16_t txnum = 0; txnum < 5; txnum++){
            CReserveKey reservekey(wallet.get());
            CAmount fee;
            int changePos = -1;
            std::string error;
            CCoinControl dummy;

        	CValidationState state;
        	CWalletTx wtx;
            BOOST_CHECK(wallet->CreateTransaction({recipient}, wtx, reservekey, fee, changePos, error, dummy));
            BOOST_TEST_MESSAGE(strprintf("%s", error));
            BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, nullptr, state));
            BOOST_TEST_MESSAGE(strprintf("%s%s (code %i)",
                    state.GetRejectReason(),
                    state.GetDebugMessage().empty() ? "" : ", "+state.GetDebugMessage(),
                    state.GetRejectCode()));
            wtxs.push_back(wtx);
        }

        std::vector<CMutableTransaction> blocktxs;
        for(uint16_t idx = 0; idx < wtxs.size(); idx++){
        	CMutableTransaction blocktx;
            LOCK(wallet->cs_wallet);
            blocktx = CMutableTransaction(*wallet->mapWallet.at(wtxs[idx].GetHash()).tx);
        }

        CreateAndProcessBlock(blocktxs, {}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

        for(uint16_t txidx = 0; txidx < wtxs.size(); txidx++){
            LOCK(wallet->cs_wallet);
            auto it = wallet->mapWallet.find(wtxs[txidx].GetHash());
            BOOST_CHECK(it != wallet->mapWallet.end());
            it->second.SetMerkleBranch(chainActive.Tip(), 1 + txidx);
        }
    }

    void AddTx()
    {

    	const CChainParams& chainparams = Params();
		CWalletTx wtx;
		std::vector<CRecipient> vecSend;
		CReserveKey reservekey(wallet.get());
		CAmount fee;
		int changePos = -1;
		std::string error;
		CCoinControl dummy;
		CValidationState state;

//		for(uint16_t txCreateNum = 0; txCreateNum < chainparams.GetConsensus().CoinbaseMaturity * chainparams.GetConsensus().MaxFreshStakePerBlock; txCreateNum++){
//			CScript scriptTx = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
//			vecSend.push_back({scriptTx, 1 * COIN, false});
//		}

		CScript scriptTx = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
		vecSend.push_back({scriptTx, 1 * COIN, false});

        BOOST_CHECK(wallet->CreateTransaction(vecSend, wtx, reservekey, fee, changePos, error, dummy));
        BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, nullptr, state));

        CMutableTransaction blocktx;
        {
            LOCK(wallet->cs_wallet);
            blocktx = CMutableTransaction(*wallet->mapWallet.at(wtx.GetHash()).tx);
        }

        CreateAndProcessBlock({CMutableTransaction(blocktx)}, {}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

        {
            LOCK(wallet->cs_wallet);
            auto it = wallet->mapWallet.find(wtx.GetHash());
            BOOST_CHECK(it != wallet->mapWallet.end());
            it->second.SetMerkleBranch(chainActive.Tip(), 1);
        }
    }

    void AddTxStx()
    {
    	const CChainParams& chainparams = Params();
    	CBlockIndex* pindexPrev = chainActive.Tip();
    	CAmount ticketPrice = 0;
        estimateNextStakeDifficultyV2(chainparams.GetConsensus(), pindexPrev, ticketPrice);

        CWalletTx wtx;
        std::vector<CWalletTx> swtxVch;
        uint32_t stxTotalNum = chainparams.GetConsensus().MaxFreshStakePerBlock;
        swtxVch.resize(stxTotalNum);
        CReserveKey reservekey(wallet.get());
        CAmount fee;
        std::string error;
        CCoinControl dummy;
        CValidationState state;

        for(uint16_t num = 0; num < stxTotalNum; num++){
        	CWalletTx swtx;
        	BOOST_CHECK(wallet->CreateTicketPurchaseTx(swtx, ticketPrice, fee, error, dummy));
        	BOOST_TEST_MESSAGE(strprintf("%s", error));
        	BOOST_CHECK(wallet->CommitTransaction(swtx, reservekey, nullptr, state));
            BOOST_TEST_MESSAGE(strprintf("%s%s (code %i)",
                    state.GetRejectReason(),
                    state.GetDebugMessage().empty() ? "" : ", "+state.GetDebugMessage(),
                    state.GetRejectCode()));
			swtxVch[num] = swtx;
			state.clear();
        }

        std::vector<CMutableTransaction> sblocktxVch;
        {
            LOCK(wallet->cs_wallet);
            CMutableTransaction sblocktx;
            for(uint16_t txnum = 0; txnum < swtxVch.size(); txnum++){
            	sblocktx = CMutableTransaction(*wallet->mapWalletStx.at(swtxVch[txnum].GetHash()).tx);
            	sblocktxVch.push_back(sblocktx);
            }
        }
        CreateAndProcessBlock({}, sblocktxVch, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

        {
            LOCK(wallet->cs_wallet);

            for(uint16_t txnum = 0; txnum < swtxVch.size(); txnum++){
                auto sit = wallet->mapWalletStx.find(swtxVch[txnum].GetHash());
                BOOST_CHECK(sit != wallet->mapWalletStx.end());
                sit->second.SetMerkleBranch(chainActive.Tip(), 1 + txnum);
            }
        }
    }

    std::unique_ptr<CWallet> wallet;
};

BOOST_FIXTURE_TEST_SUITE(wallet_stx_tests, ListCoinsTestingSetup)


BOOST_AUTO_TEST_CASE(ListCoins)
{
    std::string coinbaseAddress = coinbaseKey.GetPubKey().GetID().ToString();

    // Confirm ListCoins initially returns 1 coin grouped under coinbaseKey
    // address.
    auto list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 1);

    // Check initial balance from one mature coinbase transaction.
    BOOST_CHECK_EQUAL(20000 * COIN, wallet->GetAvailableBalance());

    // Add a transaction creating a change address, and confirm ListCoins still
    // returns the coin associated with the change address underneath the
    // coinbaseKey pubkey, even though the change address has a different
    // pubkey.
    AddTx();
    list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 3);

	AddTxStx();

    list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 4);

    // Lock both coins. Confirm number of available coins drops to 0.
    std::vector<COutput> available;
    wallet->AvailableCoins(available);
    BOOST_CHECK_EQUAL(available.size(), 4);
    for (const auto& group : list) {
        for (const auto& coin : group.second) {
            LOCK(wallet->cs_wallet);
            wallet->LockCoin(COutPoint(coin.tx->GetHash(), coin.i));
        }
    }
    wallet->AvailableCoins(available);
    BOOST_CHECK_EQUAL(available.size(), 0);

    // Confirm ListCoins still returns same result as before, despite coins
    // being locked.
    list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 4);
}

BOOST_AUTO_TEST_CASE(wallet_stx_vote_block)	// test many txs use one utxo
{
	CBlockIndex* prevIdx = chainActive.Tip();
	CBlockIndex* blockIdx = nullptr;
	const CChainParams& chainparams = Params();

    std::string coinbaseAddress = coinbaseKey.GetPubKey().GetID().ToString();

    for(uint16_t bnum = 0; bnum < Params().GetConsensus().TicketMaturity * 2; bnum++){
    	AddTxStx();
    }
    auto list2 = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list2.size(), 1);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list2.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list2.begin()->second.size(), 9);



}

BOOST_AUTO_TEST_SUITE_END()
