// Author: yaopengfei(yuitta@163.com)
// this file is ported from decred staketx.go
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_STAKE_STAKETX_H
#define BITCOIN_STAKE_STAKETX_H


#include <tinyformat.h>
#include <primitives/transaction.h>
#include <primitives/block.h>
#include <chain.h>

#include <util.h>
#include <string>

// MaxInputsPerSStx is the maximum number of inputs allowed in an SStx.
const int MaxInputsPerSStx = 64;

// MaxOutputsPerSStx is the maximum number of outputs allowed in an SStx;
// you need +1 for the tagged SStx output.
const int MaxOutputsPerSStx = MaxInputsPerSStx*2 + 1;

// NumInputsPerSSGen is the exact number of inputs for an SSGen
// (stakebase) tx.  Inputs are a tagged SStx output and a stakebase (null)
// input.
const int NumInputsPerSSGen = 2; // SStx and stakebase

// MaxOutputsPerSSGen is the maximum number of outputs in an SSGen tx,
// which are all outputs to the addresses specified in the OP_RETURNs of
// the original SStx referenced as input plus reference and vote
// OP_RETURN outputs in the zeroeth and first position.
const int MaxOutputsPerSSGen = MaxInputsPerSStx + 2;

// NumInputsPerSSRtx is the exact number of inputs for an SSRtx (stake
// revocation tx); the only input should be the SStx output.
const int NumInputsPerSSRtx = 1;

// MaxOutputsPerSSRtx is the maximum number of outputs in an SSRtx, which
// are all outputs to the addresses specified in the OP_RETURNs of the
// original SStx referenced as input plus a reference to the block header
// hash of the block in which voting was missed.
const int MaxOutputsPerSSRtx = MaxInputsPerSStx;

// SStxPKHMinOutSize is the minimum size of of an OP_RETURN commitment output
// for an SStx tx.
// 20 bytes P2SH/P2PKH + 8 byte amount + 4 byte fee range limits
const int SStxPKHMinOutSize = 32;

// SStxPKHMaxOutSize is the maximum size of of an OP_RETURN commitment output
// for an SStx tx.
const int SStxPKHMaxOutSize = 77;

// SSGenBlockReferenceOutSize is the size of a block reference OP_RETURN
// output for an SSGen tx.
const int SSGenBlockReferenceOutSize = 38;

// SSGenVoteBitsOutputMinSize is the minimum size for a VoteBits push
// in an SSGen.
const int SSGenVoteBitsOutputMinSize = 4;

// SSGenVoteBitsOutputMaxSize is the maximum size for a VoteBits push
// in an SSGen.
const int SSGenVoteBitsOutputMaxSize = 77;

// MaxSingleBytePushLength is the largest maximum push for an
// SStx commitment or VoteBits push.
const int MaxSingleBytePushLength = 75;

// SSGenVoteBitsExtendedMaxSize is the maximum size for a VoteBitsExtended
// push in an SSGen.
//
// The final vote transaction includes a single data push for all vote
// bits concatenated.  The non-extended vote bits occupy the first 2
// bytes, thus the max number of extended vote bits is the maximum
// allow length for a single byte data push minus the 2 bytes required
// by the non-extended vote bits.
const int SSGenVoteBitsExtendedMaxSize = MaxSingleBytePushLength - 2;

// SStxVoteReturnFractionMask extracts the return fraction from a
// commitment output version.
// If after applying this mask &0x003f is given, the entire amount of
// the output is allowed to be spent as fees if the flag to allow fees
// is set.
const int SStxVoteReturnFractionMask = 0x003f;

// SStxRevReturnFractionMask extracts the return fraction from a
// commitment output version.
// If after applying this mask &0x3f00 is given, the entire amount of
// the output is allowed to be spent as fees if the flag to allow fees
// is set.
const int SStxRevReturnFractionMask = 0x3f00;

// SStxVoteFractionFlag is a bitflag mask specifying whether or not to
// apply a fractional limit to the amount used for fees in a vote.
// 00000000 00000000 = No fees allowed
// 00000000 01000000 = Apply fees rule
const int SStxVoteFractionFlag = 0x0040;

// SStxRevFractionFlag is a bitflag mask specifying whether or not to
// apply a fractional limit to the amount used for fees in a vote.
// 00000000 00000000 = No fees allowed
// 01000000 00000000 = Apply fees rule
const int SStxRevFractionFlag = 0x4000;

// VoteConsensusVersionAbsent is the value of the consensus version
// for a short read of the voteBits.
const int VoteConsensusVersionAbsent = 0;

using vec64 = std::vector<int64_t>;
using valtype = std::vector<unsigned char>;
using VoteVersionTuple = std::pair<uint32_t, uint16_t>;

// validSStxAddressOutPrefix is the valid prefix for a 30-byte
// minimum OP_RETURN push for a commitment for an SStx.
// Example SStx address out:
// 0x6a (OP_RETURN)
// 0x1e (OP_DATA_30, push length: 30 bytes)
//
// 0x?? 0x?? 0x?? 0x?? (20 byte public key hash)
// 0x?? 0x?? 0x?? 0x??
// 0x?? 0x?? 0x?? 0x??
// 0x?? 0x?? 0x?? 0x??
// 0x?? 0x??
//
// 0x?? 0x?? 0x?? 0x?? (8 byte amount)
// 0x?? 0x?? 0x?? 0x??
//
// 0x?? 0x??           (2 byte range limits)
const valtype validSStxAddressOutMinPrefix(0x62, 0x1e);

// validSSGenReferenceOutPrefix is the valid prefix for a block
// reference output for an SSGen tx.
// Example SStx address out:
// 0x6a (OP_RETURN)
// 0x28 (OP_DATA_40, push length: 40 bytes)
//
// 0x?? 0x?? 0x?? 0x?? (32 byte block header hash for the block
// 0x?? 0x?? 0x?? 0x??   you wish to vote on)
// 0x?? 0x?? 0x?? 0x??
// 0x?? 0x?? 0x?? 0x??
// 0x?? 0x?? 0x?? 0x??
// 0x?? 0x?? 0x?? 0x??
// 0x?? 0x?? 0x?? 0x??
// 0x?? 0x?? 0x?? 0x??
//
// 0x?? 0x?? 0x?? 0x?? (4 byte uint32 for the height of the block
//                      that you wish to vote on)
const valtype validSSGenReferenceOutPrefix(0x6a, 0x24);

// validSSGenVoteOutMinPrefix is the valid prefix for a vote output for an
// SSGen tx.
// 0x6a (OP_RETURN)
// 0x02 (OP_DATA_2 to OP_DATA_75, push length: 2-75 bytes)
//
// 0x?? 0x?? (VoteBits) ... 0x??
const valtype validSSGenVoteOutMinPrefix(0x6a, 0x02);

/** Capture information about block/transaction validation */
class CValidationStakeState {
private:
    enum mode_state {
        MODE_VALID,   //!< everything ok
        MODE_INVALID, //!< network rule violation (DoS value may be set)
        MODE_ERROR,   //!< run-time error
    } mode;
    int nDoS;
    std::string strRejectReason;
    unsigned int chRejectCode;
    bool corruptionPossible;
    std::string strDebugMessage;
public:
    CValidationStakeState() : mode(MODE_VALID), nDoS(0), chRejectCode(0), corruptionPossible(false) {}
    bool DoS(int level, bool ret = false,
             unsigned int chRejectCodeIn=0, const std::string &strRejectReasonIn="",
             bool corruptionIn=false,
             const std::string &strDebugMessageIn="") {
        chRejectCode = chRejectCodeIn;
        strRejectReason = strRejectReasonIn;
        corruptionPossible = corruptionIn;
        strDebugMessage = strDebugMessageIn;
        if (mode == MODE_ERROR)
            return ret;
        nDoS += level;
        mode = MODE_INVALID;
        return ret;
    }
    bool Invalid(bool ret = false,
                 unsigned int _chRejectCode=0, const std::string &_strRejectReason="",
                 const std::string &_strDebugMessage="") {
        return DoS(0, ret, _chRejectCode, _strRejectReason, false, _strDebugMessage);
    }
    bool Error(const std::string& strRejectReasonIn) {
        if (mode == MODE_VALID)
            strRejectReason = strRejectReasonIn;
        mode = MODE_ERROR;
        return false;
    }
    bool IsValid() const {
        return mode == MODE_VALID;
    }
    bool IsInvalid() const {
        return mode == MODE_INVALID;
    }
    bool IsError() const {
        return mode == MODE_ERROR;
    }
    bool IsInvalid(int &nDoSOut) const {
        if (IsInvalid()) {
            nDoSOut = nDoS;
            return true;
        }
        return false;
    }
    bool CorruptionPossible() const {
        return corruptionPossible;
    }
    void SetCorruptionPossible() {
        corruptionPossible = true;
    }
    /** Convert CValidationStakeState to a human-readable message for logging */
    std::string FormatStateMessage()
    {
        return strprintf("%s%s (code %i)",
            GetRejectReason(),
            GetDebugMessage().empty() ? "" : ", " + GetDebugMessage(),
            GetRejectCode());
    }
    unsigned int GetRejectCode() const { return chRejectCode; }
    std::string GetRejectReason() const { return strRejectReason; }
    std::string GetDebugMessage() const { return strDebugMessage; }
    void clear() {
    	nDoS = 0;
    	chRejectCode = 0;
    	corruptionPossible = false;
    	mode = MODE_VALID;
    	strRejectReason = "";
    	strDebugMessage = "";

    }


};

bool IsSSGen(const CTransaction& tx, CValidationStakeState& state);
bool CheckSSgen(const CTransaction& tx, CValidationStakeState& state);
uint16_t SSGenVoteBits(const CTransaction& tx);
void SSGenBlockVotedOn(const CTransaction& tx, uint256& hash, uint32_t& height);
bool isNullOutpoint(const CTransaction& tx);
uint32_t SSGenVersion(const CTransaction& tx);
//bool isNullFraudProof(const CTransaction& tx);
bool IsStakeBase(const CTransaction& tx);

bool IsSStx(const CTransaction& tx, CValidationStakeState &state);
bool CheckSStx(const CTransaction& tx, CValidationStakeState &state);
vec64 CalculateRewards(vec64 amounts, int64_t amountTicket, int64_t subsidy);
bool SStxNullOutputAmounts(vec64 amounts, vec64 changeAmounts, int64_t amountTicket, int64_t& fees, vec64& contribAmounts, CValidationStakeState& state);

bool IsSSRtx(const CTransaction& tx, CValidationStakeState &state);
bool CheckSSRtx(const CTransaction& tx, CValidationStakeState &state);

bool FindSpentTicketsInBlock(const CBlock& block, SpentTicketsInBlock& ticketinfo, CValidationStakeState& state);


#endif //BITCOIN_STAKE_STAKETX_H
