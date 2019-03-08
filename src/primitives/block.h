// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>
#include <wallet/test/wallet_stx_def.h>

#include <string>

static const int SER_WITHOUT_SIGNATURE = 1 << 3;

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
public:
    // header
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;
    uint256 hashStateRoot; // qtum
    uint256 hashUTXORoot; // qtum
    // proof-of-stake specific fields
    COutPoint prevoutStake;
    std::vector<unsigned char> vchBlockSig;

    //////////////////////////////////////////////////////////////// decred
    // Number of participating voters for this block.
    uint16_t Voters;

    // Number of new sstx in this block.
    uint8_t FreshStake;

    // Number of ssrtx present in this block.
    uint8_t Revocations;

    // Size of the ticket pool.
    uint32_t PoolSize;

    // Stake difficulty target.
    int64_t sBits;

    // Votes on the previous merkleroot and yet undecided parameters.
    uint16_t nVoteBits;
    ////////////////////////////////////////////////////////////////

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        READWRITE(hashStateRoot); // qtum
        READWRITE(hashUTXORoot); // qtum
        READWRITE(prevoutStake);
        if (!(s.GetType() & SER_WITHOUT_SIGNATURE))
            READWRITE(vchBlockSig);
        if(TestStxDebug){
        	READWRITE(Voters);
        	READWRITE(FreshStake);
        	READWRITE(Revocations);
        	READWRITE(PoolSize);
        	READWRITE(sBits);
        	READWRITE(nVoteBits);
        }
    }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        hashStateRoot.SetNull(); // qtum
        hashUTXORoot.SetNull(); // qtum
        vchBlockSig.clear();
        prevoutStake.SetNull();

        Voters = 0;
        FreshStake = 0;
        Revocations = 0;
        PoolSize = 0;
        sBits = 0;
        nVoteBits = 0;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;

    std::string GetHex() const;

    uint256 GetHashWithoutSign() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }
    
    // ppcoin: two types of block: proof-of-work or proof-of-stake
    virtual bool IsProofOfStake() const //qtum
    {
        return !prevoutStake.IsNull();
    }

    virtual bool IsProofOfWork() const
    {
        return !IsProofOfStake();
    }
    
    virtual uint32_t StakeTime() const
    {
        uint32_t ret = 0;
        if(IsProofOfStake())
        {
            ret = nTime;
        }
        return ret;
    }

    CBlockHeader& operator=(const CBlockHeader& other) //qtum
    {
        if (this != &other)
        {
            this->nVersion       = other.nVersion;
            this->hashPrevBlock  = other.hashPrevBlock;
            this->hashMerkleRoot = other.hashMerkleRoot;
            this->nTime          = other.nTime;
            this->nBits          = other.nBits;
            this->nNonce         = other.nNonce;
            this->hashStateRoot  = other.hashStateRoot;
            this->hashUTXORoot   = other.hashUTXORoot;
            this->vchBlockSig    = other.vchBlockSig;
            this->prevoutStake   = other.prevoutStake;
            if(TestStxDebug){
                this->Voters = other.Voters;
                this->FreshStake = other.FreshStake;
                this->Revocations = other.Revocations;
                this->PoolSize = other.PoolSize;
                this->sBits = other.sBits;
                this->nVoteBits = other.nVoteBits;
            }
        }
        return *this;
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    //////////////////////////////////////////////////////////////// decred
    std::vector<CTransactionRef> svtx;

    ////////////////////////////////////////////////////////////////


    // memory only
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *((CBlockHeader*)this) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CBlockHeader*)this);
        READWRITE(vtx);
        if(TestStxDebug){
        	READWRITE(svtx);
        }
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        svtx.clear();
        fChecked = false;
    }

    std::pair<COutPoint, unsigned int> GetProofOfStake() const //qtum
    {
        return IsProofOfStake()? std::make_pair(prevoutStake, nTime) : std::make_pair(COutPoint(), (unsigned int)0);
    }
    
    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        block.hashStateRoot  = hashStateRoot; // qtum
        block.hashUTXORoot   = hashUTXORoot; // qtum
        block.vchBlockSig    = vchBlockSig;
        block.prevoutStake   = prevoutStake;
        if(TestStxDebug){
        	block.Voters = Voters;
        	block.FreshStake = FreshStake;
        	block.Revocations = Revocations;
        	block.PoolSize = PoolSize;
        	block.sBits = sBits;
        	block.nVoteBits = nVoteBits;
        }
        return block;
    }

    std::string ToString() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(const std::vector<uint256>& vHaveIn) : vHave(vHaveIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // BITCOIN_PRIMITIVES_BLOCK_H
