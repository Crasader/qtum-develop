/*
 * lottery.cpp
 *
 *  Created on: Dec 19, 2018
 *	this file is ported from decred lottery.go
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <stake/lottery.h>
#include <util.h>
#include <crypto/common.h>
#include <crypto/sha256.h>

// CalcHash256PRNGIV calculates and returns the initialization vector for a
// given seed.  This can be used in conjunction with the NewHash256PRNGFromIV
// function to arrive at the same values that are produced when calling
// NewHash256PRNG with the seed.
uint256 CalcHash256PRNGIV(std::vector<unsigned char>& seed){
	uint256 hashOut;
	CSHA256().Write(seed.data(), seed.size()).Write(seedConst.data(), seedConst.size()).Finalize(hashOut.begin());
	return hashOut;
}

// NewHash256PRNGFromIV returns a deterministic pseudorandom number generator
// that uses a 256-bit secure hashing function to generate random uint32s given
// an initialization vector.  The CalcHash256PRNGIV can be used to calculate an
// initialization vector for a given seed such that the generator will produce
// the same values as if NewHash256PRNG were called with the same seed.  This
// allows callers to cache and reuse the initialization vector for a given seed
// to avoid recomputation.
Hash256PRNG NewHash256PRNGFromIV(uint256 hash) {
	// idx and lastHash are automatically initialized
	// as 0.  We initialize the seed by appending a constant
	// to it and hashing to give 32 bytes. This ensures
	// that regardless of the input, the PRNG is always
	// doing a short number of rounds because it only
	// has to hash < 64 byte messages.  The constant is
	// derived from the hexadecimal representation of
	// pi.
	Hash256PRNG hp;
	hp.seed = hash;
	hp.lastHash = hp.seed;
	hp.idx = 0;
	return hp;
}

// NewHash256PRNG returns a deterministic pseudorandom number generator that
// uses a 256-bit secure hashing function to generate random uint32s given a
// seed.
Hash256PRNG NewHash256PRNG(std::vector<unsigned char>& seed){
	return NewHash256PRNGFromIV(CalcHash256PRNGIV(seed));
}

// StateHash returns a hash referencing the current state the deterministic PRNG. TODO change the algorithm
void Hash256PRNG::StateHash(uint256& hashOut){
	uint256 fhash = lastHash;
	uint64_t fIdx = idx;
	int32_t fHashIdx = hashIdx;
	unsigned char cIdx[4] = {};
	WriteBE32(cIdx, (uint32_t)fIdx);
	unsigned char chashIdx[4] = {};
	memcpy(chashIdx, &fHashIdx, 4);
	CSHA256().Write(fhash.begin(), fhash.size()).Write(cIdx, 4).Write(chashIdx, 1).Finalize(hashOut.begin());
}

// Hash256Rand returns a uint32 random number using the pseudorandom number
// generator and updates the state.
uint32_t Hash256PRNG::Hash256Rand(){
	uint32_t ran;
	memcpy(&ran, lastHash.begin() + hashIdx, 4);
	ran = htobe32(ran);
	hashIdx++;

	// 'roll over' the hash index to use and store it.
	if(hashIdx > 7){
		unsigned char cIdx[4] = {};
		WriteBE32(cIdx, (uint32_t)idx);
		CSHA256().Write(seed.begin(), seed.size()).Write(cIdx, 4).Finalize(lastHash.begin());
		idx++;
		hashIdx = 0;
	}

	// 'roll over' the PRNG by re-hashing the seed when
	// we overflow idx.
	if(idx > 0xFFFFFFFF){
		CSHA256().Write(seed.begin(), seed.size()).Finalize(seed.begin());
		lastHash = seed;
		idx = 0;
	}

	return ran;
}

// uniformRandom returns a random in the range [0 ... upperBound) while avoiding
// modulo bias, thus giving a normal distribution within the specified range.
//
// Ported from
// https://github.com/conformal/clens/blob/master/src/arc4random_uniform.c
uint32_t Hash256PRNG::uniformRandom(uint32_t upperBound){
	uint32_t ran;
	uint32_t min;
	if(upperBound < 2){
		return 0;
	}

	if(upperBound > 0x80000000){
		min = ~upperBound + 1;
	} else {
		// (2**32 - (x * 2)) % x == 2**32 % x when x <= 2**31
		min = ((0xFFFFFFFF - (upperBound * 2)) + 1) % upperBound;
	}

	while(true){
		ran = Hash256Rand();
		if(ran >= min)
			break;
	}

	return (ran % upperBound);
}

// findTicketIdxs finds n many unique index numbers for a list length size.
bool findTicketIdxs(int32_t size, uint16_t n, Hash256PRNG& prng, std::vector<int32_t>& lis){
	if(size < n){
		return error("%s: list size too small: %d < %d(target)", __func__, size, n);
	}

	int64_t max = 0xFFFFFFFF;
	if((int64_t)size > max){
		return error("%s: list size too big: %d > %d(limit)", __func__, size, max);
	}

	uint32_t sz = size;
	uint16_t listLen = 0;
	std::vector<int32_t>::iterator it;
	while(listLen < n){
		int32_t ran = prng.uniformRandom(sz);
		it = std::find(lis.begin(), lis.end(), ran);
		if(it == lis.end()){
			lis.push_back(ran);
			listLen++;
		}
	}

	return true;
}

// fetchWinners is a ticket database specific function which iterates over the
// entire treap and finds winners at selected indexes.  These are returned
// as a slice of pointers to keys, which can be recast as []*chainhash.Hash.
// Importantly, it maintains the list of winners in the same order as specified
// in the original idxs passed to the function.
bool fetchWinners(std::vector<int32_t> idxs, Immutable& t,  std::vector<uint256>& winners){
	uint256 hash;
	uint32_t height;
	uint8_t flag;

	if(idxs.size() == 0)
		return error("%s, empty idxs vector", __func__);

	if(t.Len() == 0)
		return error("%s: missing or empty treap", __func__);


	for(int32_t idx : idxs){
		if(idx < 0 || idx >= t.Len()){
			return error("%s: idx %d out of bounds", __func__, idx);
		}

		if(idx < t.Len()){
			if(t.GetByIndex(idx, hash, height, flag) == false)
				return error("%s: GetByIndex fail to find idx %d", __func__, idx);
			winners.push_back(hash);
		}
	}

	return true;
}
