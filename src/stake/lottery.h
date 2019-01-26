/*
 * lottery.h
 *
 *  Created on: Dec 19, 2018
 *	this file is ported from decred lottery.go
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef BITCOIN_STAKE_LOTTERY_H_
#define BITCOIN_STAKE_LOTTERY_H_

#include <uint256.h>
#include <stake/tickettreap/common.h>

#include <vector>
#include <stdint.h>

// seedConst is a constant derived from the hex representation of pi. It
// is used along with a caller-provided seed when initializing
// the deterministic lottery prng.
static const std::vector<unsigned char> seedConst = {0x24, 0x3F, 0x6A, 0x88, 0x85, 0xA3, 0x08, 0xD3};

class Hash256PRNG;
uint256 CalcHash256PRNGIV(std::vector<unsigned char> seed);
Hash256PRNG NewHash256PRNGFromIV(uint256 hash);
Hash256PRNG NewHash256PRNG(std::vector<unsigned char> seed);
bool findTicketIdxs(int32_t size, uint16_t n, Hash256PRNG& prng, std::vector<int32_t>& lis);
bool fetchWinners(std::vector<int32_t> idxs, Immutable& t,  std::vector<uint256>& winners);


// Hash256PRNG is a determinstic pseudorandom number generator that uses a
// 256-bit secure hashing function to generate random uint32s starting from
// an initial seed.
class Hash256PRNG{

	friend Hash256PRNG NewHash256PRNGFromIV(uint256 hash);

public:
	explicit Hash256PRNG(): seed(uint256()), hashIdx(0), idx(0), lastHash(uint256()) {}

	void StateHash(uint256& hashOut);
	uint32_t Hash256Rand();
	uint32_t uniformRandom(uint32_t upperBound);


private:
	uint256 seed;		// The seed used to initialize
	int32_t hashIdx;	// Position in the cached hash
	uint64_t idx;		// Position in the hash iterator
	uint256 lastHash;	// Cached last hash used
};


#endif /* BITCOIN_STAKE_LOTTERY_H_ */
