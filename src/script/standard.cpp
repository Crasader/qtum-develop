// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/standard.h>

#include <pubkey.h>
#include <script/script.h>
#include <util.h>
#include <utilstrencodings.h>

#include <qtum/qtumstate.h>
#include <qtum/qtumtransaction.h>
#include <validation.h>

#include <cmath>

typedef std::vector<unsigned char> valtype;

bool fAcceptDatacarrier = DEFAULT_ACCEPT_DATACARRIER;
unsigned nMaxDatacarrierBytes = MAX_OP_RETURN_RELAY;

CScriptID::CScriptID(const CScript& in) : uint160(Hash160(in.begin(), in.end())) {}

const char* GetTxnOutputType(txnouttype t)
{
    switch (t)
    {
    case TX_NONSTANDARD: return "nonstandard";
    case TX_PUBKEY: return "pubkey";
    case TX_PUBKEYHASH: return "pubkeyhash";
    case TX_SCRIPTHASH: return "scripthash";
    case TX_MULTISIG: return "multisig";
    case TX_NULL_DATA: return "nulldata";
    case TX_WITNESS_V0_KEYHASH: return "witness_v0_keyhash";
    case TX_WITNESS_V0_SCRIPTHASH: return "witness_v0_scripthash";
    case TX_WITNESS_UNKNOWN: return "witness_unknown";
    case TX_CREATE: return "create";
    case TX_CALL: return "call";
    case TX_STAKEGEN: return "stakegen";
    case TX_STAKETX_SUBMMISSION: return "staketx_submission";
    case TX_STAKETX_REVOCATION: return "staketx_revocation";
    case TX_STAKETX_CHANGE: return "staket_change";
    }
    return nullptr;
}

bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<std::vector<unsigned char> >& vSolutionsRet, bool contractConsensus)
{
    //contractConsesus is true when evaluating if a contract tx is "standard" for consensus purposes
    //It is false in all other cases, so to prevent a particular contract tx from being broadcast on mempool, but allowed in blocks,
    //one should ensure that contractConsensus is false

    // Templates
    static std::multimap<txnouttype, CScript> mTemplates;
    if (mTemplates.empty())
    {
        // Standard tx, sender provides pubkey, receiver adds signature
        mTemplates.insert(std::make_pair(TX_PUBKEY, CScript() << OP_PUBKEY << OP_CHECKSIG));

        // Bitcoin address tx, sender provides hash of pubkey, receiver provides signature and pubkey
        mTemplates.insert(std::make_pair(TX_PUBKEYHASH, CScript() << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));

        // Sender provides N pubkeys, receivers provides M signatures
        mTemplates.insert(std::make_pair(TX_MULTISIG, CScript() << OP_SMALLINTEGER << OP_PUBKEYS << OP_SMALLINTEGER << OP_CHECKMULTISIG));

        // ssGen tx
		mTemplates.insert(std::make_pair(TX_STAKEGEN, CScript() << OP_SSGEN << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));
		mTemplates.insert(std::make_pair(TX_STAKEGEN, CScript() << OP_SSGEN << OP_HASH160 << OP_PUBKEYHASH << OP_EQUAL));

		// ssTx submission
		mTemplates.insert(std::make_pair(TX_STAKETX_SUBMMISSION, CScript() << OP_SSTX << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));
		mTemplates.insert(std::make_pair(TX_STAKETX_SUBMMISSION, CScript() << OP_SSTX << OP_HASH160 << OP_PUBKEYHASH << OP_EQUAL));

		// ssTx Revocation
		mTemplates.insert(std::make_pair(TX_STAKETX_REVOCATION, CScript() << OP_SSRTX << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));
		mTemplates.insert(std::make_pair(TX_STAKETX_REVOCATION, CScript() << OP_SSRTX << OP_HASH160 << OP_PUBKEYHASH << OP_EQUAL));

		// ssTx Change
		mTemplates.insert(std::make_pair(TX_STAKETX_CHANGE, CScript() << OP_SSTXCHANGE << OP_DUP << OP_HASH160 << OP_PUBKEYHASH << OP_EQUALVERIFY << OP_CHECKSIG));
		mTemplates.insert(std::make_pair(TX_STAKETX_CHANGE, CScript() << OP_SSTXCHANGE << OP_HASH160 << OP_PUBKEYHASH << OP_EQUAL));

        // Contract creation tx
        mTemplates.insert(std::make_pair(TX_CREATE, CScript() << OP_VERSION << OP_GAS_LIMIT << OP_GAS_PRICE << OP_DATA << OP_CREATE));

        // Call contract tx
        mTemplates.insert(std::make_pair(TX_CALL, CScript() << OP_VERSION << OP_GAS_LIMIT << OP_GAS_PRICE << OP_DATA << OP_PUBKEYHASH << OP_CALL));

    }

    vSolutionsRet.clear();

    // Shortcut for pay-to-script-hash, which are more constrained than the other types:
    // it is always OP_HASH160 20 [20 byte hash] OP_EQUAL
    if (scriptPubKey.IsPayToScriptHash())
    {
        typeRet = TX_SCRIPTHASH;
        std::vector<unsigned char> hashBytes(scriptPubKey.begin()+2, scriptPubKey.begin()+22);
        vSolutionsRet.push_back(hashBytes);
        return true;
    }

    int witnessversion;
    std::vector<unsigned char> witnessprogram;
    if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
        if (witnessversion == 0 && witnessprogram.size() == 20) {
            typeRet = TX_WITNESS_V0_KEYHASH;
            vSolutionsRet.push_back(witnessprogram);
            return true;
        }
        if (witnessversion == 0 && witnessprogram.size() == 32) {
            typeRet = TX_WITNESS_V0_SCRIPTHASH;
            vSolutionsRet.push_back(witnessprogram);
            return true;
        }
        if (witnessversion != 0) {
            typeRet = TX_WITNESS_UNKNOWN;
            vSolutionsRet.push_back(std::vector<unsigned char>{(unsigned char)witnessversion});
            vSolutionsRet.push_back(std::move(witnessprogram));
            return true;
        }
        return false;
    }

    // Provably prunable, data-carrying output
    //
    // So long as script passes the IsUnspendable() test and all but the first
    // byte passes the IsPushOnly() test we don't care what exactly is in the
    // script.
    if (scriptPubKey.size() >= 1 && scriptPubKey[0] == OP_RETURN && scriptPubKey.IsPushOnly(scriptPubKey.begin()+1)) {
        typeRet = TX_NULL_DATA;
        return true;
    }

    // Scan templates
    const CScript& script1 = scriptPubKey;
    for (const std::pair<txnouttype, CScript>& tplate : mTemplates)
    {
        const CScript& script2 = tplate.second;
        vSolutionsRet.clear();

        opcodetype opcode1, opcode2;
        std::vector<unsigned char> vch1, vch2;

        VersionVM version;
        version.rootVM=20; //set to some invalid value

        // Compare
        CScript::const_iterator pc1 = script1.begin();
        CScript::const_iterator pc2 = script2.begin();
        while (true)
        {
            if (pc1 == script1.end() && pc2 == script2.end())
            {
                // Found a match
                typeRet = tplate.first;
                if (typeRet == TX_MULTISIG)
                {
                    // Additional checks for TX_MULTISIG:
                    unsigned char m = vSolutionsRet.front()[0];
                    unsigned char n = vSolutionsRet.back()[0];
                    if (m < 1 || n < 1 || m > n || vSolutionsRet.size()-2 != n)
                        return false;
                }
                return true;
            }
            if (!script1.GetOp(pc1, opcode1, vch1))
                break;
            if (!script2.GetOp(pc2, opcode2, vch2))
                break;

            // Template matching opcodes:
            if (opcode2 == OP_PUBKEYS)
            {
                while (vch1.size() >= 33 && vch1.size() <= 65)
                {
                    vSolutionsRet.push_back(vch1);
                    if (!script1.GetOp(pc1, opcode1, vch1))
                        break;
                }
                if (!script2.GetOp(pc2, opcode2, vch2))
                    break;
                // Normal situation is to fall through
                // to other if/else statements
            }

            if (opcode2 == OP_PUBKEY)
            {
                if (vch1.size() < 33 || vch1.size() > 65)
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_PUBKEYHASH)
            {
                if (vch1.size() != sizeof(uint160))
                    break;
                vSolutionsRet.push_back(vch1);
            }
            else if (opcode2 == OP_SMALLINTEGER)
            {   // Single-byte small integer pushed onto vSolutions
                if (opcode1 == OP_0 ||
                    (opcode1 >= OP_1 && opcode1 <= OP_16))
                {
                    char n = (char)CScript::DecodeOP_N(opcode1);
                    vSolutionsRet.push_back(valtype(1, n));
                }
                else
                    break;
            }
            /////////////////////////////////////////////////////////// qtum
            else if (opcode2 == OP_VERSION)
            {
                if(0 <= opcode1 && opcode1 <= OP_PUSHDATA4)
                {
                    if(vch1.empty() || vch1.size() > 4 || (vch1.back() & 0x80))
                        return false;

                    version = VersionVM::fromRaw(CScriptNum::vch_to_uint64(vch1));
                    if(!(version.toRaw() == VersionVM::GetEVMDefault().toRaw() || version.toRaw() == VersionVM::GetNoExec().toRaw())){
                        // only allow standard EVM and no-exec transactions to live in mempool
                        return false;
                    }
                }
            }
            else if(opcode2 == OP_GAS_LIMIT) {
                try {
                    uint64_t val = CScriptNum::vch_to_uint64(vch1);
                    if(contractConsensus) {
                        //consensus rules (this is checked more in depth later using DGP)
                        if (version.rootVM != 0 && val < 1) {
                            return false;
                        }
                        if (val > MAX_BLOCK_GAS_LIMIT_DGP) {
                            //do not allow transactions that could use more gas than is in a block
                            return false;
                        }
                    }else{
                        //standard mempool rules for contracts
                        //consensus rules for contracts
                        if (version.rootVM != 0 && val < STANDARD_MINIMUM_GAS_LIMIT) {
                            return false;
                        }
                        if (val > DEFAULT_BLOCK_GAS_LIMIT_DGP / 2) {
                            //don't allow transactions that use more than 1/2 block of gas to be broadcast on the mempool
                            return false;
                        }

                    }
                }
                catch (const scriptnum_error &err) {
                    return false;
                }
            }
            else if(opcode2 == OP_GAS_PRICE) {
                try {
                    uint64_t val = CScriptNum::vch_to_uint64(vch1);
                    if(contractConsensus) {
                        //consensus rules (this is checked more in depth later using DGP)
                        if (version.rootVM != 0 && val < 1) {
                            return false;
                        }
                    }else{
                        //standard mempool rules
                        if (version.rootVM != 0 && val < STANDARD_MINIMUM_GAS_PRICE) {
                            return false;
                        }
                    }
                }
                catch (const scriptnum_error &err) {
                    return false;
                }
            }
            else if(opcode2 == OP_DATA)
            {
                if(0 <= opcode1 && opcode1 <= OP_PUSHDATA4)
                {
                    if(vch1.empty())
                        break;
                }
            }
            ///////////////////////////////////////////////////////////
            else if (opcode1 != opcode2 || vch1 != vch2)
            {
                // Others must match exactly
                break;
            }
        }
    }

    vSolutionsRet.clear();
    typeRet = TX_NONSTANDARD;
    return false;
}

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet, txnouttype *typeRet)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    if(typeRet){
        *typeRet = whichType;
    }

    if (whichType == TX_PUBKEY)
    {
        CPubKey pubKey(vSolutions[0]);
        if (!pubKey.IsValid())
            return false;

        addressRet = pubKey.GetID();
        return true;
    }
    else if (whichType == TX_PUBKEYHASH)
    {
        addressRet = CKeyID(uint160(vSolutions[0]));
        return true;
    }
    else if (whichType == TX_SCRIPTHASH)
    {
        addressRet = CScriptID(uint160(vSolutions[0]));
        return true;
    } else if (whichType == TX_WITNESS_V0_KEYHASH) {
        WitnessV0KeyHash hash;
        std::copy(vSolutions[0].begin(), vSolutions[0].end(), hash.begin());
        addressRet = hash;
        return true;
    } else if (whichType == TX_WITNESS_V0_SCRIPTHASH) {
        WitnessV0ScriptHash hash;
        std::copy(vSolutions[0].begin(), vSolutions[0].end(), hash.begin());
        addressRet = hash;
        return true;
    } else if (whichType == TX_WITNESS_UNKNOWN) {
        WitnessUnknown unk;
        unk.version = vSolutions[0][0];
        std::copy(vSolutions[1].begin(), vSolutions[1].end(), unk.program);
        unk.length = vSolutions[1].size();
        addressRet = unk;
        return true;
    }
    // Multisig txns have more than one address...
    return false;
}

bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet)
{
    addressRet.clear();
    typeRet = TX_NONSTANDARD;
    std::vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, typeRet, vSolutions))
        return false;
    if (typeRet == TX_NULL_DATA){
        // This is data, not addresses
        return false;
    }

    if (typeRet == TX_MULTISIG)
    {
        nRequiredRet = vSolutions.front()[0];
        for (unsigned int i = 1; i < vSolutions.size()-1; i++)
        {
            CPubKey pubKey(vSolutions[i]);
            if (!pubKey.IsValid())
                continue;

            CTxDestination address = pubKey.GetID();
            addressRet.push_back(address);
        }

        if (addressRet.empty())
            return false;
    }
    else
    {
        nRequiredRet = 1;
        CTxDestination address;
        if (!ExtractDestination(scriptPubKey, address))
           return false;
        addressRet.push_back(address);
    }

    return true;
}

namespace
{
class CScriptVisitor : public boost::static_visitor<bool>
{
private:
    CScript *script;
public:
    explicit CScriptVisitor(CScript *scriptin) { script = scriptin; }

    bool operator()(const CNoDestination &dest) const {
        script->clear();
        return false;
    }

    bool operator()(const CKeyID &keyID) const {
        script->clear();
        *script << OP_DUP << OP_HASH160 << ToByteVector(keyID) << OP_EQUALVERIFY << OP_CHECKSIG;
        return true;
    }

    bool operator()(const CScriptID &scriptID) const {
        script->clear();
        *script << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
        return true;
    }

    bool operator()(const WitnessV0KeyHash& id) const
    {
        script->clear();
        *script << OP_0 << ToByteVector(id);
        return true;
    }

    bool operator()(const WitnessV0ScriptHash& id) const
    {
        script->clear();
        *script << OP_0 << ToByteVector(id);
        return true;
    }

    bool operator()(const WitnessUnknown& id) const
    {
        script->clear();
        *script << CScript::EncodeOP_N(id.version) << std::vector<unsigned char>(id.program, id.program + id.length);
        return true;
    }
};
} // namespace

CScript GetScriptForDestination(const CTxDestination& dest)
{
    CScript script;

    boost::apply_visitor(CScriptVisitor(&script), dest);
    return script;
}

CScript GetScriptForRawPubKey(const CPubKey& pubKey)
{
    return CScript() << std::vector<unsigned char>(pubKey.begin(), pubKey.end()) << OP_CHECKSIG;
}

CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys)
{
    CScript script;

    script << CScript::EncodeOP_N(nRequired);
    for (const CPubKey& key : keys)
        script << ToByteVector(key);
    script << CScript::EncodeOP_N(keys.size()) << OP_CHECKMULTISIG;
    return script;
}

CScript GetScriptForWitness(const CScript& redeemscript)
{
    CScript ret;

    txnouttype typ;
    std::vector<std::vector<unsigned char> > vSolutions;
    if (Solver(redeemscript, typ, vSolutions)) {
        if (typ == TX_PUBKEY) {
            return GetScriptForDestination(WitnessV0KeyHash(Hash160(vSolutions[0].begin(), vSolutions[0].end())));
        } else if (typ == TX_PUBKEYHASH) {
            return GetScriptForDestination(WitnessV0KeyHash(vSolutions[0]));
        }
    }
    uint256 hash;
    CSHA256().Write(&redeemscript[0], redeemscript.size()).Finalize(hash.begin());
    return GetScriptForDestination(WitnessV0ScriptHash(hash));
}

//////////////////////////////////////////////////////////////// decred

CScript PayToSStx(CKeyID& stakeaddress){
	return CScript() << OP_SSTX << OP_DUP << OP_HASH160 << ToByteVector(stakeaddress) << OP_EQUALVERIFY << OP_CHECKSIG;
}

CScript PayToSStxChange(CKeyID& stakeaddress){
	return CScript() << OP_SSTXCHANGE << OP_DUP << OP_HASH160 << ToByteVector(stakeaddress) << OP_EQUALVERIFY << OP_CHECKSIG;
}

CScript PayToSSGen(CKeyID& stakeaddress){
	return CScript() << OP_SSGEN << OP_DUP << OP_HASH160 << ToByteVector(stakeaddress) << OP_EQUALVERIFY << OP_CHECKSIG;
}

CScript PayToSSRtx(CKeyID& stakeaddress){
	return CScript() << OP_SSRTX << OP_DUP << OP_HASH160 << ToByteVector(stakeaddress) << OP_EQUALVERIFY << OP_CHECKSIG;
}

CScript voteBlockScript(CBlockIndex* parentBlock){
	return VoteCommitmentScript(parentBlock->phashBlock, parentBlock->nHeight);
}

CScript VoteCommitmentScript(const uint256* phash, int32_t nheight){
	unsigned char chr[36];
	memcpy(chr, phash->begin(), phash->size());
	WriteLE32(chr + 32, (uint32_t)nheight);
	return CScript() << OP_RETURN << std::vector<unsigned char>(chr, chr + 36);
}

CScript voteBitsScript(uint16_t bits){
	unsigned char chr[2];
	WriteLE16(chr, bits);
	return CScript() << OP_RETURN << std::vector<unsigned char>(chr, chr + 2);
}


CScript PurchaseCommitmentScript(CKeyID& address, CAmount&  amount, CAmount& voteFeeLimit, CAmount& revocationFeeLimit){
	// The limits are defined in terms of the closest base 2 exponent and
	// a bit that must be set to specify the limit is to be applied.  The
	// vote fee exponent is in the bottom 8 bits, while the revocation fee
	// exponent is in the upper 8 bits.
	uint16_t limits = 0;
	if(voteFeeLimit != 0){
		uint16_t exp = (uint16_t)std::ceil(std::log2((double)voteFeeLimit));
		limits |= (exp | 0x40);
	}
	if(revocationFeeLimit != 0){
		uint16_t exp = (uint16_t)std::ceil(std::log2((double)revocationFeeLimit));
		limits |= ((exp | 0x40) << 8);
	}
	// The data consists of the 20-byte raw script address for the given
	// address, 8 bytes for the amount to commit to (with the upper bit flag
	// set to indicate a pay-to-script-hash address), and 2 bytes for the
	// fee limits.
	std::vector<unsigned char> vchrs;
	vchrs.assign(address.begin(), address.end());
	vchrs.resize(30);
	WriteLE64(vchrs.data() + 20, (uint64_t)amount);
	vchrs[27] |= 1 << 7;

	WriteLE16(vchrs.data() + 28, (uint64_t)limits);
	CScript script;
	script << OP_RETURN << vchrs;

	return script;
}

bool GetPubkHashfromScript(const CScript& senderScript, CKeyID& addrOut){
    std::vector<valtype> vSolutions;
    txnouttype whichType;
    if (!Solver(senderScript, whichType, vSolutions)){
    	return error("%s: bad script format", __func__);
    }
    switch (whichType){
    case TX_PUBKEYHASH:
    case TX_STAKETX_SUBMMISSION:
    case TX_STAKETX_CHANGE:
    case TX_STAKEGEN:
    case TX_STAKETX_REVOCATION:
    	addrOut = CKeyID(uint160(vSolutions[0]));
    	break;

    case TX_PUBKEY:
    	addrOut = CPubKey(vSolutions[0]).GetID();
    	break;
    default:
    	return error("%s: other format not support, will fix it further", __func__);

    }
    return true;
}

////////////////////////////////////////////////////////////////

bool IsValidDestination(const CTxDestination& dest) {
    return dest.which() != 0;
}

bool IsValidContractSenderAddress(const CTxDestination &dest)
{
    const CKeyID *keyID = boost::get<CKeyID>(&dest);
    return keyID != 0;
}
