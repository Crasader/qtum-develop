/*
 * rpc_stx_tests.cpp
 *
 *  Created on: Apr 15, 2019
 *  Author: yaopengfei(yuitta@163.com)
 *  Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <rpc/server.h>
#include <rpc/client.h>
#include <wallet/test/wallet_test_fixture.h>
#include <validation.h>
#include <consensus/validation.h>
#include <wallet/coincontrol.h>

#include <base58.h>
#include <core_io.h>
#include <netbase.h>

#include <test/test_bitcoin.h>

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

#include <univalue.h>


UniValue CallStxRPC(std::string args)
{
    std::vector<std::string> vArgs;
    boost::split(vArgs, args, boost::is_any_of(" \t"));
    std::string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    JSONRPCRequest request;
    request.strMethod = strMethod;
    request.params = RPCConvertValues(strMethod, vArgs);
    request.fHelp = false;
    BOOST_CHECK(tableRPC[strMethod]);
    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        UniValue result = (*method)(request);
        return result;
    }
    catch (const UniValue& objError) {
        throw std::runtime_error(find_value(objError, "message").get_str());
    }
}

static void AddKey(CWallet& wallet, const CKey& key)
{
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

class WalletRPCTestingSetup : public TestChain100SetupTmp
{
public:
	WalletRPCTestingSetup()
    {
        CreateAndProcessBlock({}, {}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        ::bitdb.MakeMock();
        g_address_type = OUTPUT_TYPE_DEFAULT;
        g_change_type = OUTPUT_TYPE_DEFAULT;
        wallet.reset(new CWallet(std::unique_ptr<CWalletDBWrapper>(new CWalletDBWrapper(&bitdb, "wallet_test.dat"))));
        bool firstRun;
        wallet->LoadWallet(firstRun);
        RegisterValidationInterface(wallet.get());
        RegisterWalletRPCCommands(tableRPC);

        AddKey(*wallet, coinbaseKey);
        WalletRescanReserver reserver(wallet.get());
        reserver.reserve();
        wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver);
        wallet->SetBroadcastTransactions(true);

        vpwallets.insert(vpwallets.begin(), wallet.get());
    }

    ~WalletRPCTestingSetup()
    {
    	UnregisterValidationInterface(wallet.get());
        wallet.reset();
        ::bitdb.Flush(true);
        ::bitdb.Reset();
    }

    void AddTx()
    {

		CWalletTx wtx;
		std::vector<CRecipient> vecSend;
		CReserveKey reservekey(wallet.get());
		CAmount fee;
		int changePos = -1;
		std::string error;
		CCoinControl dummy;
		CValidationState state;

		CScript scriptTx = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
		vecSend.push_back({scriptTx, 1 * COIN, false});

        BOOST_CHECK(wallet->CreateTransaction(vecSend, wtx, reservekey, fee, changePos, error, dummy));
        BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, nullptr, state));

        auto beforIt = mempool.mapTx.find(wtx.tx->GetHash());
        BOOST_CHECK(beforIt != mempool.mapTx.end());

        CMutableTransaction blocktx;
        {
            LOCK(wallet->cs_wallet);
            blocktx = CMutableTransaction(*wallet->mapWallet.at(wtx.GetHash()).tx);
        }

        CreateAndProcessBlock({CMutableTransaction(blocktx)}, {}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

        auto afterIt = mempool.mapTx.find(wtx.tx->GetHash());
        BOOST_CHECK(afterIt == mempool.mapTx.end());
        {
            LOCK(wallet->cs_wallet);
            auto it = wallet->mapWallet.find(wtx.GetHash());
            BOOST_CHECK(it != wallet->mapWallet.end());
            it->second.SetMerkleBranch(chainActive.Tip(), 1);
        }
    }

    std::unique_ptr<CWallet> wallet;
};


BOOST_FIXTURE_TEST_SUITE(rpc_stx_tests, WalletRPCTestingSetup)

BOOST_AUTO_TEST_CASE(rpc_stx){
	std::string coinbaseAddress = coinbaseKey.GetPubKey().GetID().ToString();

	// Confirm ListCoins initially returns 1 coin grouped under coinbaseKey
	// address.
	auto list = wallet->ListCoins();
	BOOST_CHECK_EQUAL(list.size(), 1);
	BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
	BOOST_CHECK_EQUAL(list.begin()->second.size(), 1);

    // Add a transaction creating a change address, and confirm ListCoins still
    // returns the coin associated with the change address underneath the
    // coinbaseKey pubkey, even though the change address has a different
    // pubkey.
    AddTx();
    list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1);
    BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 3);


    // Test raw transaction API argument handling
    UniValue r;

    r = CallStxRPC("createticketpurchase");
    std::string txid = r.get_str();

    BOOST_TEST_MESSAGE(txid);

    std::string inputStr = "getrawtransaction ";
    inputStr = inputStr + r.get_str() + " true";
    r = CallStxRPC(inputStr);

    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "txid").get_str(), txid);

	// Confirm ListCoins initially returns 1 coin grouped under coinbaseKey
	// address.
	list = wallet->ListCoins();
	BOOST_CHECK_EQUAL(list.size(), 1);
	BOOST_CHECK_EQUAL(boost::get<CKeyID>(list.begin()->first).ToString(), coinbaseAddress);
	BOOST_CHECK_EQUAL(list.begin()->second.size(), 3);
}







BOOST_AUTO_TEST_SUITE_END()
