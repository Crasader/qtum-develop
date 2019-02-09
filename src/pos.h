// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef QUANTUM_POS_H
#define QUANTUM_POS_H

#include <chain.h>
#include <primitives/transaction.h>
#include <consensus/validation.h>
#include <txdb.h>
#include <validation.h>
#include <arith_uint256.h>
#include <hash.h>
#include <timedata.h>
#include <chainparams.h>
#include <script/sign.h>
#include <consensus/consensus.h>

// To decrease granularity of timestamp
// Supposed to be 2^n-1
static const uint32_t STAKE_TIMESTAMP_MASK = 15;

struct CStakeCache{
    CStakeCache(uint32_t blockFromTime_, CAmount amount_) : blockFromTime(blockFromTime_), amount(amount_){
    }
    uint32_t blockFromTime;
    CAmount amount;
};

void CacheKernel(std::map<COutPoint, CStakeCache>& cache, const COutPoint& prevout, CBlockIndex* pindexPrev, CCoinsViewCache& view);

// Compute the hash modifier for proof-of-stake
uint256 ComputeStakeModifier(const CBlockIndex* pindexPrev, const uint256& kernel);

// Check whether stake kernel meets hash target
// Sets hashProofOfStake on success return
bool CheckStakeKernelHash(CBlockIndex* pindexPrev, unsigned int nBits, uint32_t blockFromTime, CAmount prevoutAmount, const COutPoint& prevout, unsigned int nTimeTx, uint256& hashProofOfStake, uint256& targetProofOfStake, bool fPrintProofOfStake=false);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(CBlockIndex* pindexPrev, CValidationState& state, const CTransaction& tx, unsigned int nBits, uint32_t nTimeBlock, uint256& hashProofOfStake, uint256& targetProofOfStake, CCoinsViewCache& view);

// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(uint32_t nTimeBlock);

// Wrapper around CheckStakeKernelHash()
// Also checks existence of kernel input and min age
// Convenient for searching a kernel
bool CheckKernel(CBlockIndex* pindexPrev, unsigned int nBits, uint32_t nTimeBlock, const COutPoint& prevout, CCoinsViewCache& view);
bool CheckKernel(CBlockIndex* pindexPrev, unsigned int nBits, uint32_t nTimeBlock, const COutPoint& prevout, CCoinsViewCache& view, const std::map<COutPoint, CStakeCache>& cache);

// estimateNextStakeDifficulty estimates the next stake difficulty using the
// algorithm defined in DCP0001 by pretending the provided number of tickets
// will be purchased in the remainder of the interval unless the flag to use max
// tickets is set in which case it will use the max possible number of tickets
// that can be purchased in the remainder of the interval.
//
// This function MUST be called with the chain state lock held (for writes).
bool estimateNextStakeDifficulty(const Consensus::Params& consensusParams, const CBlockIndex* curNode, int64_t newTickets, bool useMaxTickets, int64_t& sBits);

// calcNextRequiredStakeDifficultyV2 calculates the required stake difficulty
// for the block after the passed previous block node based on the algorithm
// defined in DCP0001.
//
// This function MUST be called with the chain state lock held (for writes).
bool estimateNextStakeDifficultyV2(const Consensus::Params& consensusParams, const CBlockIndex* curNode, int64_t& sBits);

// calcNextStakeDiff calculates the next stake difficulty for the given set
// of parameters using the algorithm defined in DCP0001.
//
// This function contains the heart of the algorithm and thus is separated for
// use in both the actual stake difficulty calculation as well as estimation.
//
// The caller must perform all of the necessary chain traversal in order to
// get the current difficulty, previous retarget interval's pool size plus
// its immature tickets, as well as the current pool size plus immature tickets.
//
// This function is safe for concurrent access.
int64_t calcNextStakeDiff(const Consensus::Params& consensusParams, int32_t nHeight, int64_t curDiff, int64_t prevPoolSizeAll, int64_t curPoolSizeAll);


#endif // QUANTUM_POS_H
