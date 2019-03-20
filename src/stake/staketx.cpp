// Author: yaopengfei(yuitta@163.com)
// this file is ported from decred staketx.go
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <stake/staketx.h>


#include <script/script_error.h>
#include <script/standard.h>
#include <script/script.h>
#include <prevector.h>
#include <vector>
#include <crypto/common.h>
#include <openssl/bn.h>
#include <utilstrencodings.h>

bool isNullOutpoint(const CTransaction& tx) {
	COutPoint nullInOP = tx.vin[0].prevout;
	if (nullInOP.n == UINT32_MAX && nullInOP.hash == uint256()) {
		return true;
	}
	return false;
}

// isNullFraudProof determines whether or not a previous transaction fraud proof
// is set.
//bool isNullFraudProof(const CTransaction& tx){
//	CTxIn txIn = tx.vin[0];
//	if(txIn){
//		return false;
//	}
//}

// IsStakeBase returns whether or not a tx could be considered as having a
// topically valid stake base present.
bool IsStakeBase(const CTransaction& tx) {
	if (tx.vin.size() != 2)
		return false;

	if (!isNullOutpoint(tx)) {
		return false;
	}
	return true;
}

// SSGenVoteBits takes an SSGen tx as input and scans through its
// outputs, returning the VoteBits of the index 1 output.
//
// This function is only safe to be called on a transaction that
// has passed IsSSGen.
uint16_t SSGenVoteBits(const CTransaction& tx){
	return ReadLE16(tx.vout[1].scriptPubKey.data() + 2);
}

// SSGenVersion takes an SSGen tx as input and returns the network
// consensus version from the VoteBits output.  If there is a short
// read, the network consensus version is considered 0 or "unset".
//
// This function is only safe to be called on a transaction that
// has passed IsSSGen.
uint32_t SSGenVersion(const CTransaction& tx){
	if(tx.vout[1].scriptPubKey.size() < 8){
		return VoteConsensusVersionAbsent;
	}
	return ReadLE32(tx.vout[1].scriptPubKey.data() + 4);
}

// IsSSGen returns whether or not a transaction is a stake submission generation
// transaction.  There are also known as votes.
bool IsSSGen(const CTransaction& tx, CValidationStakeState& state) {
	return(CheckSSgen(tx, state));
}

// generation transaction.  It does some simple validation steps to make sure
// the number of inputs, number of outputs, and the input/output scripts are
// valid.
//
// This does NOT check to see if the subsidy is valid or whether or not the
// value of input[0] + subsidy = value of the outputs.
//
// SSGen transactions are specified as below.
// Inputs:
// Stakebase null input [index 0]
// SStx-tagged output [index 1]
//
// Outputs:
// OP_RETURN push of 40 bytes containing: [index 0]
//     i. 32-byte block header of block being voted on.
//     ii. 8-byte int of this block's height.
// OP_RETURN push of 2 bytes containing votebits [index 1]
// SSGen-tagged output to address from SStx-tagged output's tx index output 1
//     [index 2]
// SSGen-tagged output to address from SStx-tagged output's tx index output 2
//     [index 3]
// ...
// SSGen-tagged output to address from SStx-tagged output's tx index output
//     MaxInputsPerSStx [index MaxOutputsPerSSgen - 1]
bool CheckSSgen(const CTransaction& tx, CValidationStakeState& state) {
	if (tx.vin.size() != NumInputsPerSSGen) {
		return state.Invalid(false, STX_REJECT_INVALID, "ssgen-input",
				"invalid number of inputs");
	}

	if (tx.vout.size() > MaxOutputsPerSSGen) {
		return state.Invalid(false, STX_REJECT_INVALID, "ssgen-output",
				"invalid number of outputs");
	}

	if (tx.vout.size() == 0) {
		return state.Invalid(false, STX_REJECT_INVALID, "ssgen-output",
				"no many outputs");
	}

	if (!IsStakeBase(tx)) {
		return state.Invalid(false, STX_REJECT_INVALID, "ssgen-format",
				"SSGen tx did not include a stakebase in the zeroeth input position");
	}

    std::vector<valtype> vSolutions;
    txnouttype whichType;
	const CScript& zeroethOutputScript = tx.vout[0].scriptPubKey;
	Solver(zeroethOutputScript, whichType, vSolutions);
	if(whichType != TX_NULL_DATA){
		return state.Invalid(false, STX_REJECT_INVALID, "ssgen-output",
				"First SSGen output should have been an OP_RETURN data push, but was not");
	}
	whichType = TX_NONSTANDARD;
	if(zeroethOutputScript.size() != SSGenBlockReferenceOutSize){
		return state.Invalid(false, STX_REJECT_INVALID, "ssgen-output", "First SSGen output should have been 38 bytes long, but was not");
	}

	if (!zeroethOutputScript.HasOpReturnB()) {
		return state.Invalid(false, STX_REJECT_INVALID, "ssgen-output",
				"First SSGen output had aninvalid prefix");
	}

	const CScript& firstOutputScript = tx.vout[1].scriptPubKey;
	Solver(firstOutputScript, whichType, vSolutions);
	if(whichType != TX_NULL_DATA){
		return state.Invalid(false, STX_REJECT_INVALID, "ssgen-output",
				"Second SSGen output should have been an OP_RETURN data push, but was not");
	}
	whichType = TX_NONSTANDARD;
	// The length of the output script should be between 4 and 77 bytes long.
	if (firstOutputScript.size() < SSGenVoteBitsOutputMinSize
			|| firstOutputScript.size() > SSGenVoteBitsOutputMaxSize) {
		return state.Invalid(false, STX_REJECT_INVALID, "ssgen-output",
				"SSGen votebits output at output index 1 was a NullData (OP_RETURN) push of the wrong size");
	}

	CScript::const_iterator pbegin = firstOutputScript.begin();
	uint8_t pushLen = *(++pbegin);
	bool pushLengthValid = pushLen >= validSSGenVoteOutMinPrefix[1] && pushLen <= MaxSingleBytePushLength;
	// The first byte should be OP_RETURN, while the second byte should be a
	// valid push length.
	if(!firstOutputScript.HasOpReturnB() || !pushLengthValid) {
		return state.Invalid(false, STX_REJECT_INVALID, "ssgen-output",
				"Second SSGen output had an invalid prefix");
	}

	for(uint8_t outTxIndex = 2; outTxIndex < tx.vout.size(); outTxIndex++){
		const CScript& rawScript = tx.vout[outTxIndex].scriptPubKey;
		Solver(rawScript, whichType, vSolutions);
		if(whichType != TX_STAKEGEN ){
			return state.Invalid(false, STX_REJECT_INVALID, "ssgen-output",
					strprintf("SSGen tx output at output index %d was not an OP_SSGEN tagged output", outTxIndex));
		}
		whichType = TX_NONSTANDARD;
	}

	return true;
}

// SSGenBlockVotedOn takes an SSGen tx and returns the block voted on hash and height.
//
// This function is only safe to be called on a transaction that
// has passed IsSSGen.
void SSGenBlockVotedOn(const CTransaction& tx, uint256& hash, uint32_t& height){
	CScript::const_iterator pbegin = tx.vout[0].scriptPubKey.begin();
	std::vector<unsigned char> vch;
	vch.assign(pbegin+2, pbegin+34);
	hash = uint256(vch);
	height = ReadLE32(&pbegin[34]);
}

// IsSStx returns whether or not a transaction is a stake submission transaction.
// These are also known as tickets.
bool IsSStx(const CTransaction& tx, CValidationStakeState &state){
	return CheckSStx(tx, state);
}

// --------------------------------------------------------------------------------
// Stake Transaction Identification Functions
// --------------------------------------------------------------------------------

// CheckSStx returns an error if a transaction is not a stake submission
// transaction.  It does some simple validation steps to make sure the number of
// inputs, number of outputs, and the input/output scripts are valid.
//
// SStx transactions are specified as below.
// Inputs:
// untagged output 1 [index 0]
// untagged output 2 [index 1]
// ...
// untagged output MaxInputsPerSStx [index MaxInputsPerSStx-1]
//
// Outputs:
// OP_SSTX tagged output [index 0]
// OP_RETURN push of input 1's address for reward receiving [index 1]
// OP_SSTXCHANGE tagged output for input 1 [index 2]
// OP_RETURN push of input 2's address for reward receiving [index 3]
// OP_SSTXCHANGE tagged output for input 2 [index 4]
// ...
// OP_RETURN push of input MaxInputsPerSStx's address for reward receiving
//     [index (MaxInputsPerSStx*2)-2]
// OP_SSTXCHANGE tagged output [index (MaxInputsPerSStx*2)-1]
//
// The output OP_RETURN pushes should be of size 20 bytes (standard address).
bool CheckSStx(const CTransaction& tx, CValidationStakeState &state){
	// Check to make sure there aren't too many inputs.
	// CheckTransactionSanity already makes sure that number of inputs is
	// greater than 0, so no need to check that.
	if (tx.vin.size() > MaxInputsPerSStx) {
		return state.Invalid(false, STX_REJECT_INVALID, "sstx-input",
				"SStx has too many inputs");
	}

	// Check to make sure there aren't too many outputs.
	if (tx.vout.size() > MaxOutputsPerSStx) {
		return state.Invalid(false, STX_REJECT_INVALID, "sstx-output",
				"SStx has too many outputs");
	}

	// Check to make sure there are some outputs.
	if (tx.vout.size() == 0) {
		return state.Invalid(false, STX_REJECT_INVALID, "sstx-output",
				"SStx has no outputs");
	}

	std::vector<valtype> vSolutions;
	txnouttype whichType;

	// Ensure that the first output is tagged OP_SSTX.
	const CScript& ZeroOutputScript = tx.vout[0].scriptPubKey;
	Solver(ZeroOutputScript, whichType, vSolutions);
	if(whichType != TX_STAKETX_SUBMMISSION ){
		return state.Invalid(false, STX_REJECT_INVALID, "sstx-output",
				"First SStx output should have been OP_SSTX tagged, but it was not");
	}
	whichType = TX_NONSTANDARD;
	// Ensure that the number of outputs is equal to the number of inputs + 1.
	if((tx.vin.size() * 2 + 1) != tx.vout.size()){
		return state.Invalid(false, STX_REJECT_INVALID, "sstx-format",
				"The number of inputs in the SStx tx was not the number of outputs/2 - 1");
	}

	for(uint8_t outTxIndex = 1; outTxIndex < tx.vout.size(); outTxIndex++){
		const CScript& rawScript = tx.vout[outTxIndex].scriptPubKey;
		if(outTxIndex % 2 == 0){
			Solver(rawScript, whichType, vSolutions);
			if(whichType != TX_STAKETX_CHANGE){
				return state.Invalid(false, STX_REJECT_INVALID, "sstx-output",
						strprintf("SStx output at output index %d was not an sstx change output", outTxIndex));
			}
			whichType = TX_NONSTANDARD;
			continue;
		}

		// Else (odd) check commitment outputs.  The script should be a
		// NullDataTy output.
		Solver(rawScript, whichType, vSolutions);
		if(whichType != TX_NULL_DATA){
			return state.Invalid(false, STX_REJECT_INVALID, "sstx-output",
					strprintf("SStx output at output index %d was not a NullData (OP_RETURN) push", outTxIndex));
		}
		whichType = TX_NONSTANDARD;

		// The length of the output script should be between 32 and 77 bytes long.
		if(rawScript.size() < SStxPKHMinOutSize || rawScript.size() > SStxPKHMaxOutSize){
			return state.Invalid(false, STX_REJECT_INVALID, "sstx-output",
					strprintf("SStx output at output index %d was a NullData (OP_RETURN) push of the wrong size", outTxIndex));
		}

		CScript::const_iterator pbegin = rawScript.begin();
		uint8_t pushLen = *(++pbegin);
		bool pushLengthValid = pushLen >= validSStxAddressOutMinPrefix[1] && pushLen <= MaxSingleBytePushLength;
		// The first byte should be OP_RETURN, while the second byte should be a
		// valid push length.
		if(!rawScript.HasOpReturnB() || !pushLengthValid){
			return state.Invalid(false, STX_REJECT_INVALID, "sstx-output",
					strprintf("sstx commitment at output idx %d had an invalid prefix", outTxIndex));
		}

	}

	return true;
}

// SStxNullOutputAmounts takes an array of input amounts, change amounts, and a
// ticket purchase amount, calculates the adjusted proportion from the purchase
// amount, stores it in an array, then returns the array.  That is, for any given
// SStx, this function calculates the proportional outputs that any single user
// should receive.
bool SStxNullOutputAmounts(vec64 amounts, vec64 changeAmounts, int64_t amountTicket, int64_t& fees, vec64& contribAmounts, CValidationStakeState& state){
	uint16_t lengthAmounts = amounts.size();

	if(lengthAmounts != changeAmounts.size()){
		return state.Invalid(false, STX_REJECT_INVALID, "sstx-amount", "amounts was not equal in length to change amounts!");
	}

	if(amountTicket <= 0){
		return state.Invalid(false, STX_REJECT_INVALID, "sstx-amount", "committed amount was too small!");
	}

	contribAmounts.resize(lengthAmounts);
	int64_t sum = 0;

	// Now we want to get the adjusted amounts.  The algorithm is like this:
	// 1 foreach amount
	// 2     subtract change from input, store
	// 3     add this amount to sum
	// 4 check sum against the total committed amount
	for(uint16_t index = 0; index < lengthAmounts; index++){
		contribAmounts[index] = amounts[index] - changeAmounts[index];
		if(contribAmounts[index] < 0){
			return state.Invalid(false, STX_REJECT_INVALID, "sstx-amount",
					strprintf("change at idx %d spent more coins than allowed (have: %d, spent: %d)", index, amounts[index], changeAmounts[index]));
		}
		sum += contribAmounts[index];
	}
	fees = sum - amountTicket;
	return true;
}

// CalculateRewards takes a list of SStx adjusted output amounts, the amount used
// to purchase that ticket, and the reward for an SSGen tx and subsequently
// generates what the outputs should be in the SSGen tx.  If used for calculating
// the outputs for an SSRtx, pass 0 for subsidy.
vec64 CalculateRewards(vec64 amounts, int64_t amountTicket, int64_t subsidy){
	vec64 outputsAmounts(amounts.size(), 0);

	// SSGen handling
	int64_t amountWithStakebase = amountTicket + subsidy;

	// Get the sum of the amounts contributed between both fees
	// and contributions to the ticket.
	int64_t totalContrib = 0;
	for(auto amount : amounts){
		totalContrib += amount;
	}

	// Now we want to get the adjusted amounts including the reward.
	// The algorithm is like this:
	// 1 foreach amount
	// 2     amount *= 2^32
	// 3     amount /= amountTicket
	// 4     amount *= amountWithStakebase
	// 5     amount /= 2^32
	const std::string& awsbStr = i64tostr(amountWithStakebase);
	BIGNUM *amountWithStakebaseBig = 0;
	BN_dec2bn((BIGNUM **)&amountWithStakebaseBig, awsbStr.c_str());

	const std::string& tcbStr = i64tostr(totalContrib);
	BIGNUM *totalContribBig = 0;
	BN_dec2bn((BIGNUM **)&totalContribBig, tcbStr.c_str());

	for(uint32_t idx = 0; idx < amounts.size(); idx++){
		const std::string& amtStr = i64tostr(amounts[idx]);
		BIGNUM *amountBig = 0;
		BN_dec2bn((BIGNUM **)&amountBig, amtStr.c_str());

		// mul amountWithStakebase
		BN_mul(amountBig, amountBig, amountWithStakebaseBig, BN_CTX_new());

		// mul 2^32
		BN_lshift(amountBig, amountBig, 32);

		// div totalContrib
		BN_div(amountBig, NULL, amountBig, totalContribBig, BN_CTX_new());

		// div 2^32
		BN_rshift(amountBig, amountBig, 32);

		// make int64
		const std::string& oaStr(BN_bn2dec(amountBig));
		outputsAmounts[idx] = atoi64(oaStr);
	}

	return outputsAmounts;
}

// IsSSRtx returns whether or not a transaction is a stake submission revocation
// transaction.  There are also known as revocations.
bool IsSSRtx(const CTransaction& tx, CValidationStakeState &state){
	return CheckSSRtx(tx, state);
}

// CheckSSRtx returns an error if a transaction is not a stake submission
// revocation transaction.  It does some simple validation steps to make sure
// the number of inputs, number of outputs, and the input/output scripts are
// valid.
//
// SSRtx transactions are specified as below.
// Inputs:
// SStx-tagged output [index 0]
//
// Outputs:
// SSGen-tagged output to address from SStx-tagged output's tx index output 1
//     [index 0]
// SSGen-tagged output to address from SStx-tagged output's tx index output 2
//     [index 1]
// ...
// SSGen-tagged output to address from SStx-tagged output's tx index output
//     MaxInputsPerSStx [index MaxOutputsPerSSRtx - 1]
bool CheckSSRtx(const CTransaction& tx, CValidationStakeState &state){
	// Check to make sure there is the correct number of inputs.
	// CheckTransactionSanity already makes sure that number of inputs is
	// greater than 0, so no need to check that.
	if(tx.vin.size() != NumInputsPerSSRtx){
		return state.Invalid(false, STX_REJECT_INVALID, "ssrtx-input",
				"SSRtx has an invalid number of inputs");
	}

	// Check to make sure there aren't too many outputs.
	if(tx.vout.size() > MaxOutputsPerSSRtx){
		return state.Invalid(false, STX_REJECT_INVALID, "ssrtx-output",
				"SSRtx has too many outputs");
	}

	// Check to make sure there are some outputs.
	if(tx.vout.size() == 0){
		return state.Invalid(false, STX_REJECT_INVALID, "ssrtx-output",
				"SSRtx has no output");
	}

	std::vector<valtype> vSolutions;
	txnouttype whichType;

	// Ensure that the first input is an SStx tagged output.
	// TODO: Do this in validate, needs a DB and chain.

	// Ensure that the tx height given in the last 8 bytes is StakeMaturity
	// many blocks ahead of the block in which that SStx appear, otherwise
	// this ticket has failed to mature and the SStx must be invalid.
	// TODO: Do this in validate, needs a DB and chain.

	// Ensure that the outputs are OP_SSRTX tagged.
	// Ensure that the tx height given in the last 8 bytes is StakeMaturity
	// many blocks ahead of the block in which that SStx appear, otherwise
	// this ticket has failed to mature and the SStx must be invalid.
	// TODO: This is validate level stuff, do this there.

	// Ensure that the outputs are OP_SSRTX tagged.
	for(uint8_t outTxIndex = 1; outTxIndex < tx.vout.size(); outTxIndex++){
		const CScript& rawScript = tx.vout[outTxIndex].scriptPubKey;
		Solver(rawScript, whichType, vSolutions);
		if(whichType != TX_STAKETX_REVOCATION){
			return state.Invalid(false, STX_REJECT_INVALID, "ssrtx-output",
					strprintf("SSRtx output at output index %d was not an OP_SSRTX tagged output", outTxIndex));
		}
	}

	// Ensure the number of outputs is equal to the number of inputs found in
	// the original SStx.
	// TODO: Do this in validate, needs a DB and chain.

	return true;
}


// FindSpentTicketsInBlock returns information about tickets spent in a given
// block. This includes voted and revoked tickets, and the vote bits of each
// spent ticket. This is faster than calling the individual functions to
// determine ticket state if all information regarding spent tickets is needed.
//
// Note that the returned hashes are of the originally purchased *tickets* and
// **NOT** of the vote/revoke transaction.
//
// The tickets are determined **only** from the STransactions of the provided
// block and no validation is performed.
//
// This function is only safe to be called with a block that has previously
// had all header commitments validated. TODO TEST, ypf
bool FindSpentTicketsInBlock(const CBlock& block, SpentTicketsInBlock& ticketinfo, CValidationStakeState& state){
	std::vector<VoteVersionTuple> votes;
	std::vector<uint256> voters;
	std::vector<uint256> revocations;

	for(auto stx : block.svtx){
		if(IsSSGen(*stx, state)){
			voters.push_back(stx->vin[1].prevout.hash);
			votes.push_back(std::make_pair(SSGenVersion(*stx), SSGenVoteBits(*stx)));	// no version value in the script at now
			continue;
		}
		if(IsSSRtx(*stx, state)){
			revocations.push_back(stx->vin[0].prevout.hash);
			continue;
		}
	}
	ticketinfo.VotedTickets = voters;
	ticketinfo.Votes = votes;
	ticketinfo.RevokedTickets = revocations;

	return true;
}

TxType DetermineTxType(const CTransaction& tx, CValidationStakeState& state){
	if(IsSStx(tx, state)){
		return TxTypeSStx;
	}
	if(IsSSGen(tx, state)){
		return TxTypeSSGen;
	}
	if(IsSSRtx(tx, state)){
		return TxTypeSSRtx;
	}
	return TxTypeRegular;
}


