/*
 * immutable.h
 *
 *  Created on: Dec 12, 2018
 *  Author: yaopengfei(yuitta@163.com)
 *	this file is ported from decred common.go
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#ifndef BITCOIN_STAKE_TICKETTREAP_COMMON_H_
#define BITCOIN_STAKE_TICKETTREAP_COMMON_H_

#include <string>
#include <vector>
#include <stdio.h>
#include <stdint.h>
#include <memory>


// ptrSize is the number of bytes in a native pointer.
//
// nolint: vet
const uint32_t ptrSize = sizeof(nullptr_t);

// nodeValueSize is the size of the fixed-size fields of a Value,
// Height (4 bytes) plus (1 byte).
const uint32_t nodeValueSize = 5;

// nodeFieldsSize is the size the fields of each node takes excluding
// the contents of the value.
// Size = 32 (key) + 4 (height) + 1(flag) + 4 (priority) + 4 (size)
//      + 8 (left pointer) + 8 (right pointer).
const uint32_t nodeFieldsSize = 32 + 4 + 1 + 4 + 4 + 2*ptrSize;

// staticDepth is the size of the static array to use for keeping track
// of the parent stack during treap iteration.  Since a treap has a very
// high probability that the tree height is logarithmic, it is
// exceedingly unlikely that the parent stack will ever exceed this size
// even for extremely large numbers of items.
const uint16_t staticDepth = 128;

// TicketHashes is a list of ticket hashes that will mature in TicketMaturity
// many blocks from the block in which they were included.
using  TicketHashes = std::vector<uint256>;

// ticket state missed, revoked, spent, expired
const static uint8_t TICKET_STATE_MISSED		= 1 << 0;
const static uint8_t TICKET_STATE_REVOKED		= 1 << 1;
const static uint8_t TICKET_STATE_SPENT			= 1 << 2;
const static uint8_t TICKET_STATE_EXPIRED		= 1 << 3;

// the number of ticket states
const static uint8_t TICKET_STATE_NUM = 4;

// treapNode represents a node in the treap.
class treapNode {
public:

	explicit treapNode() { SetNull();}

	treapNode(treapNode&) = default;
	treapNode(const treapNode&) = default;

	void SetNull(){
		mkey = uint256();
		mheight = -1;
		mflag = 0;
		mpriority = 0;
		msize = 0;
		mleft = nullptr;
		mright = nullptr;
	}

	bool IsNull(){
		return mheight == (uint32_t)-1;	// -1 = 0xFFFFFFFF
	}

	explicit treapNode(const uint256& key, uint32_t height, uint8_t flag, uint32_t priority){
		SetNull();
		mkey = key;
		mheight = height;
		mflag = flag;
		mpriority = priority;
		msize = 1;
	}

	friend bool operator==(treapNode& a, treapNode& b){
		return a.mkey == b.mkey && a.mheight == b.mheight && a.mflag == b.mflag && a.mpriority == b.mpriority &&
				a.mright == b.mright && a.mleft == b.mleft && a.msize == b.msize;
	}

	bool isHeap();

	// nodeSize returns the number of bytes the specified node occupies including
	// the struct fields and the contents of the key and value.
	uint64_t nodeSize() const {
		return nodeFieldsSize + uint256().size() + nodeValueSize;
	}

	// leftSize returns the size of the subtree on the left-hand side, and zero if
	// there is no tree present there.
	uint32_t leftsize() const{
		if (mleft != nullptr){
			return mleft->msize;
		}
		return 0;
	}

	// rightSize returns the size of the subtree on the right-hand side, and zero if
	// there is no tree present there.
	uint32_t rightsize() const{
		if(mright != nullptr){
			return mright->msize;
		}
		return 0;
	}

	void getByIndex(int32_t idx, uint256& key, uint32_t& height, uint8_t& flag);

	uint256 mkey;
	uint32_t mheight;
	uint8_t mflag;
	uint32_t mpriority;
	uint32_t msize;
	std::shared_ptr<treapNode> mleft;
	std::shared_ptr<treapNode> mright;

};

// parentStack represents a stack of parent treap nodes that are used during
// iteration.  It consists of a static array for holding the parents and a
// dynamic overflow slice.  It is extremely unlikely the overflow will ever be
// hit during normal operation, however, since a treap's height is
// probabilistic, the overflow case needs to be handled properly.  This approach
// is used because it is much more efficient for the majority case than
// dynamically allocating heap space every time the treap is iterated.
class parentStack {
public:

	explicit parentStack(): mindex(0) {}

	// Len returns the current number of items in the stack.
	int64_t Len() const{
		return mindex;
	}

	std::shared_ptr<treapNode> At(uint64_t n){
		int64_t index = mindex - n -1;
		if(index < 0){
			return nullptr;
		}
		return mitems[index];
	}

	// Pop removes the top item from the stack.  It returns nil if the stack is
	// empty.
	std::shared_ptr<treapNode> Pop(){
		if(mindex == 0)
			return nullptr;

		mindex--;
		std::shared_ptr<treapNode> node = mitems[mindex];
		mitems.pop_back();
		return node;
	}

	// Push pushes the passed item onto the top of the stack.
	void Push(std::shared_ptr<treapNode> node){
		mitems.push_back(node);
		mindex++;
	}

private:
	int64_t mindex;
	std::vector<std::shared_ptr<treapNode>> mitems;
};

// Immutable represents a treap data structure which is used to hold ordered
// key/value pairs using a combination of binary search tree and heap semantics.
//

// In other parts of the code base, we see a traditional implementation of the
// treap with a randomised priority.
//
// Note that this treap has been modified from the original in that the
// priority, rather than being a random value, is deterministically assigned the
// monotonically increasing block height.
//
// However what is interesting is that we see similar behaviour to the treap
// structure, because the keys themselves are totally randomised, degenerate
// cases of trees with a bias for leftness or rightness cannot occur.
//
// Do make note however, if the keys were not to be randomised, this would not
// be a structure which can be relied upon to self-balance as treaps do.
//
// What motivated this alteration of the treap priority was that it can be used
// as a priority queue to discover the elements at the smallest height, which
// substantially improves application performance in one critical spot, via
// use of ForEachByHeight.
//
// All operations which result in modifying the treap return a new version of
// the treap with only the modified nodes updated.  All unmodified nodes are
// shared with the previous version.  This is extremely useful in concurrent
// applications since the caller only has to atomically replace the treap
// pointer with the newly returned version after performing any mutations.  All
// readers can simply use their existing pointer as a snapshot since the treap
// it points to is immutable.  This effectively provides O(1) snapshot
// capability with efficient memory usage characteristics since the old nodes
// only remain allocated until there are no longer any references to them.
class Immutable{
public:
	explicit Immutable(): mroot(nullptr), mcount(0), mtotalSize(0) {}
	explicit Immutable(Immutable& imuNode): mroot(imuNode.mroot), mcount(imuNode.mcount), mtotalSize(imuNode.mtotalSize) {}
	explicit Immutable(const Immutable& imuNode): mroot(imuNode.mroot), mcount(imuNode.mcount), mtotalSize(imuNode.mtotalSize) {}

	explicit Immutable(std::shared_ptr<treapNode> root, int64_t count, uint64_t totalSize): mroot(root), mcount(count), mtotalSize(totalSize){}

	friend class TicketNode;

	bool testHeap(){
		if(mroot == nullptr){
			return true;
		}
		return mroot->isHeap();
	}

	// Len returns the number of items stored in the treap. // TODO consistent type
	int64_t Len() const {
		return mcount;
	}

	// Size returns a best estimate of the total number of bytes the treap is
	// consuming including all of the fields used to represent the nodes as well as
	// the size of the keys and values.  Shared values are not detected, so the
	// returned size assumes each value is pointing to different memory.
	uint64_t Size() const {
		return mtotalSize;
	}

	// get returns the treap node that contains the passed key.  It will return nil
	// when the key does not exist.
	std::shared_ptr<treapNode> Get(uint256& key) {
		for(std::shared_ptr<treapNode> node = mroot; node != nullptr;){
			// Traverse left or right depending on the result of the
			// comparison.
			if(key < node->mkey){
				node = node->mleft;
				continue;
			} else if (node->mkey < key) {
				node = node->mright;
				continue;
			}
			// The key exists.
			return node;
		}

		// A nil node was reached which means the key does not exist.
		return nullptr;
	}

	// Has returns whether or not the passed key exists.
	bool Has(uint256& key) {
		if(Get(key)){
			return true;
		}
		return false;
	}

	// GetByIndex returns the (Key, *Value) at the given position and panics if idx
	// is out of bounds.
	bool GetByIndex(int32_t idx, uint256& key, uint32_t& height, uint8_t& flag) {
		if(mroot != nullptr){
			mroot->getByIndex(idx, key, height, flag);
			return true;
		}
		return false;
	}

	bool Put(uint256& key, uint32_t height, uint8_t flag);
	bool Delete(const uint256& key);
	void ForEach(TicketHashes& hashes);
	void FetchWinnersAndExpired(std::vector<uint32_t> idxs, uint32_t height, std::vector<uint256*>& winners, std::vector<uint256*>& expired);

private:
	std::shared_ptr<treapNode> mroot;
	int64_t mcount;

	// totalSize is the best estimate of the total size of of all data in
	// the treap including the keys, values, and node sizes.
	uint64_t mtotalSize;
};

#endif /* BITCOIN_STAKE_TICKETTREAP_COMMON_H_ */
