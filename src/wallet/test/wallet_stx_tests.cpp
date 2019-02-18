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

#include <boost/test/unit_test.hpp>
#include <univalue.h>

#include <timedata.h>
#include <random.h>

static void AddKey(CWallet& wallet, const CKey& key)
{
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

BOOST_FIXTURE_TEST_SUITE(wallet_stx_tests, WalletTestingSetup)

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
        AddKey(*wallet, coinbaseKey);
        WalletRescanReserver reserver(wallet.get());
        reserver.reserve();
        wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver);
    }

    ~ListCoinsTestingSetup()
    {
        wallet.reset();
        ::bitdb.Flush(true);
        ::bitdb.Reset();
    }

    CWalletTx& AddTx(CRecipient recipient)
    {
    	const CChainParams& chainparams = Params();
    	CBlockIndex* pindexPrev = chainActive.Tip();
    	CAmount ticketPrice = 0;
        estimateNextStakeDifficultyV2(chainparams.GetConsensus(), pindexPrev, ticketPrice);

        CWalletTx wtx;
        CWalletTx swtx;
        CReserveKey reservekey(wallet.get());
        CAmount fee;
        int changePos = -1;
        std::string error;
        CCoinControl dummy;
        BOOST_CHECK(wallet->CreateTransaction({recipient}, wtx, reservekey, fee, changePos, error, dummy));
        BOOST_CHECK(wallet->CreateTicketPurchaseTx(swtx, ticketPrice, fee, error, dummy));
        CValidationState state;
        BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, nullptr, state));
        BOOST_CHECK(wallet->CommitTransaction(swtx, reservekey, nullptr, state));
        CMutableTransaction blocktx;
        {
            LOCK(wallet->cs_wallet);
            blocktx = CMutableTransaction(*wallet->mapWallet.at(wtx.GetHash()).tx);
        }

        CMutableTransaction sblocktx;
        {
            LOCK(wallet->cs_wallet);
            blocktx = CMutableTransaction(*wallet->mapWallet.at(swtx.GetHash()).tx);
        }
        CreateAndProcessBlock({CMutableTransaction(blocktx)}, {CMutableTransaction(sblocktx)}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        LOCK(wallet->cs_wallet);
        auto it = wallet->mapWallet.find(wtx.GetHash());
        BOOST_CHECK(it != wallet->mapWallet.end());
        it->second.SetMerkleBranch(chainActive.Tip(), 1);
        return it->second;
    }

    std::unique_ptr<CWallet> wallet;
};

BOOST_FIXTURE_TEST_CASE(ListCoins, ListCoinsTestingSetup)
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
    AddTx(CRecipient{GetScriptForRawPubKey({}), 1 * COIN, false /* subtract fee */});
    list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 2);

    // Lock both coins. Confirm number of available coins drops to 0.
    std::vector<COutput> available;
    wallet->AvailableCoins(available);
    BOOST_CHECK_EQUAL(available.size(), 2);
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
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()
