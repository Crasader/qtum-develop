// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <uint256.h>
#include <limits>
#include <map>
#include <string>
#include <script/script.h>

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_CSV, // Deployment of BIP68, BIP112, and BIP113.
    DEPLOYMENT_SEGWIT, // Deployment of BIP141, BIP143, and BIP147.
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

// TokenPayout is a payout for block 1 which specifies an address and an amount
// to pay to that address in a transaction output.

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;

    /** Constant for nTimeout very far in the future. */
    static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();

    /** Special value for nStartTime indicating that the deployment is always active.
     *  This is useful for testing, as it means tests don't need to deal with the activation
     *  process (which takes at least 3 BIP9 intervals). Only tests that specifically test the
     *  behaviour during activation cannot use this. */
    static constexpr int64_t ALWAYS_ACTIVE = -1;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /** Block height at which BIP16 becomes active */
    int BIP16Height;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargeting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /** Proof of work parameters */
    uint256 powLimit;
    uint256 posLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    bool fPoSNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;
    int nLastPOWBlock;
    int nFirstMPoSBlock;
    int nMPoSRewardRecipients;
    int nFixUTXOCacheHFHeight;

    ///////////////////////////////////////////////////////////////// decred

	// MinimumStakeDiff if the minimum amount of Atoms required to purchase a
	// stake ticket.
    int64_t MinimumStakeDiff;

	// MaxStakeDiff if the max amount of Atoms required to purchase a
	// stake ticket. Added by ypf, not in dcrd.
    // MaxStakeDiff = the sum of pre-mine div  TicketPoolSize.
    int64_t MaxStakeDiff;

	// Ticket pool sizes for Decred PoS. This denotes the number of possible
	// buckets/number of different ticket numbers. It is also the number of
	// possible winner numbers there are.
    uint16_t TicketPoolSize;

    // Average number of tickets per block for Decred PoS.
    uint16_t TicketsPerBlock;

    // Number of blocks for tickets to mature (spendable at TicketMaturity+1).
    uint16_t TicketMaturity;

	// CoinbaseMaturity is the number of blocks required before newly mined
	// coins (coinbase transactions) can be spent.
    uint16_t CoinbaseMaturity;

	// StakeDiffWindowSize is the number of blocks used for each interval in
	// exponentially weighted average.
    int64_t StakeDiffWindowSize;

	// MaxFreshStakePerBlock is the maximum number of new tickets that may be
	// submitted per block.
    uint8_t MaxFreshStakePerBlock;

	// StakeValidationHeight is the height at which votes (SSGen) are required
	// to add a new block to the top of the blockchain. This height is the
	// first block that will be voted on, but will include in itself no votes.
    int64_t StakeValidationHeight;

	// StakeEnabledHeight is the height in which the first ticket could possibly
	// mature.
    uint64_t StakeEnabledHeight;

	// StakeBaseSigScript is the consensus stakebase signature script for all
	// votes on the network. This isn't signed in any way, so without forcing
	// it to be this value miners/daemons could freely change it.
    CScript StakeBaseSigScript;

	// Number of blocks for tickets to expire after they have matured. This MUST
	// be >= (StakeEnabledHeight + StakeValidationHeight).
    uint32_t TicketExpiry;

    // BaseSubsidy is the starting subsidy amount for mined blocks.
    int64_t BaseSubsidy;

    // Subsidy reduction multiplier.
    int64_t MulSubsidy;

    // Subsidy reduction divisor.
    int64_t DivSubsidy;

    // SubsidyReductionInterval is the reduction interval in blocks.
    uint16_t SubsidyReductionInterval;

	// WorkRewardProportion is the comparative amount of the subsidy given for
	// creating a block.
    uint16_t WorkRewardProportion;

	// StakeRewardProportion is the comparative amount of the subsidy given for
	// casting stake votes (collectively, per block).
    uint16_t StakeRewardProportion;

	// BlockTaxProportion is the inverse of the percentage of funds for each
	// block to allocate to the developer organization.
	// e.g. 10% --> 10 (or 1 / (1/10))
	// Special case: disable taxes with a value of 0
    uint16_t BlockTaxProportion;

    uint16_t TotalSubsidyProportions() const {
    	return WorkRewardProportion + StakeRewardProportion + BlockTaxProportion;
    }

	// BlockOneLedger specifies the list of payouts in the coinbase of
	// block height 1. If there are no payouts to be given, set this
	// to an empty slice.

    ////////////////////////////////////////////////////////////////
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
