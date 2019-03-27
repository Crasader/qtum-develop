/*
 * immutable.cpp
 *
 *  Created on: Dec 12, 2018
 *  Author: yaopengfei(yuitta@163.com)
 *	this file is ported from decred common.go
 *	Distributed under the MIT software license, see the accompanying
 *  file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */

#include <uint256.h>
#include <util.h>
#include <stake/tickettreap/common.h>

#include <algorithm>
#include <stdio.h>

// isHeap tests whether the treap meets the min-heap invariant.
bool treapNode::isHeap(){

	bool left = (mleft == nullptr) || ((mleft->mpriority >= mpriority) && mleft->isHeap());
	bool right = (mright == nullptr) || ((mright->mpriority >= mpriority) && mright->isHeap());

	return left && right;
}

// getByIndex returns the (Key, *Value) at the given position and panics if idx is
// out of bounds.
void treapNode::getByIndex(int32_t idx, uint256& key, uint32_t& height, uint8_t& flag) {
	assert(idx >= 0 && idx <= int32_t(msize));
	treapNode node = *this;
	while(true){
		if(node.mleft == 0){
			if(idx == 0){
				key = node.mkey;
				height = node.mheight;
				flag = node.mflag;
				return;
			}
			node = *(node.mright);
			idx = idx -1;
		}
		else{
			if(idx < int32_t(node.mleft->msize)){
				node = *(node.mleft);
			}
			else if(idx == int32_t(node.mleft->msize)){
				key = node.mkey;
				height = node.mheight;
				flag = node.mflag;
				return;
			}
			else {
				idx = idx - int32_t(node.mleft->msize) - 1;
				node = *(node.mright);
			}
		}
	}
}

// Put inserts the passed key/value pair.  Passing a nil value will result in a
// NOOP.
bool Immutable::Put(uint256& key, uint32_t height, uint8_t flag){
//	if(!height && !flag){
//		return false;
//	}

	// The node is the root of the tree if there isn't already one.
	if(mroot == nullptr){
		mroot = std::make_shared<treapNode>(key, height, flag, height);	//TODO priority right?
		mcount = 1;
		mtotalSize = mroot->nodeSize();
		return error("%s: the tree have no node", __func__);
	}

	// Find the binary tree insertion point and construct a replaced list of
	// parents while doing so.  This is done because this is an immutable
	// data structure so regardless of where in the treap the new key/value
	// pair ends up, all ancestors up to and including the root need to be
	// replaced.
	//
	// When the key matches an entry already in the treap, replace the node
	// with a new one that has the new value set and return.
	bool Isleft;
	parentStack parents;
	for(std::shared_ptr<treapNode> comNode = mroot; comNode != nullptr;){

		parents.Push(comNode);

		// Traverse left or right depending on the result of comparing
		// the keys.
		if(key < comNode->mkey){
			comNode = comNode->mleft;
			Isleft = true;
			continue;
		} else if (comNode->mkey < key) {
			comNode = comNode->mright;
			Isleft = false;
			continue;
		}
		// The key already exists, so update its value.
		comNode->mheight = height;
		comNode->mflag = flag;

		// Return new immutable treap with the replaced node and
		// ancestors up to and including the root of the tree.
		std::shared_ptr<treapNode> newRoot = parents.At(parents.Len() - 1);
		mroot = newRoot;
		return true;
	}

	// Recompute the size member of all parents, to account for inserted item.
	std::shared_ptr<treapNode> node = std::make_shared<treapNode>(key, height, flag, height);	//TODO priority right?
	for(int64_t num = 0; num < parents.Len(); num++){
		std::shared_ptr<treapNode> pnode = parents.At(num);
		pnode->msize++;
	}

	// Link the new node into the binary tree in the correct position.
	std::shared_ptr<treapNode> parent = parents.At(0);
	if(Isleft){
		parent->mleft = node;
	} else {
		parent->mright = node;
	}

	// Perform any rotations needed to maintain the min-heap and replace
	// the ancestors up to and including the tree root.
	std::shared_ptr<treapNode> newRoot = parents.At(parents.Len() - 1);
	while(parents.Len() > 0){
		// There is nothing left to do when the node's priority is
		// greater than or equal to its parent's priority.
		std::shared_ptr<treapNode> parent = parents.Pop();
		if(node->mpriority >= parent->mpriority){
			break;
		}

		// Perform a right rotation if the node is on the left side or
		// a left rotation if the node is on the right side.
		if(parent->mleft == node){
			/* just to help visualise right-rotation...
			*         p               n
			*        / \       ->    / \
			*       n  p.r         n.l  p
			*      / \                 / \
			*    n.l n.r             n.r p.r*/
			node->msize += 1 + parent->rightsize();
			parent->msize -= 1 + node->leftsize();
			parent->mleft = node->mright;
			node->mright = parent;
		} else {
			node->msize += 1 + parent->leftsize();
			parent->msize -= 1 + node->rightsize();
			parent->mright = node->mleft;
			node->mleft = parent;
		}

		// Either set the new root of the tree when there is no
		// grandparent or relink the grandparent to the node based on
		// which side the old parent the node is replacing was on.
		std::shared_ptr<treapNode> grandparent = parents.At(0);
		if(grandparent == nullptr){
			newRoot = node;
		} else if (grandparent->mleft == parent){
			grandparent->mleft = node;
		} else {
			grandparent->mright = node;
		}
	}

	mroot = newRoot;
	mcount++;
	mtotalSize += node->nodeSize();
	return true;
}

// Delete removes the passed key from the treap and returns the resulting treap
// if it exists.  The original immutable treap is returned if the key does not
// exist.
bool Immutable::Delete(const uint256& key){
	// Find the node for the key while constructing a list of parents while
	// doing so.
	parentStack parents;
	std::shared_ptr<treapNode> delNode = nullptr;
	for(std::shared_ptr<treapNode> comNode = mroot; comNode != nullptr;){
		parents.Push(comNode);

		// Traverse left or right depending on the result of the
		// comparison.
		if(key < comNode->mkey){
			comNode = comNode->mleft;
			continue;
		} else if (comNode->mkey < key) {
			comNode = comNode->mright;
			continue;
		}

		delNode = comNode;
		break;
	}
	// There is nothing to do if the key does not exist.
	if (delNode == nullptr){
		return error("%s: the key does not exist", __func__);
	}

	// When the only node in the tree is the root node and it is the one
	// being deleted, there is nothing else to do besides removing it.
	std::shared_ptr<treapNode> parent = parents.At(1);
	if(parent == nullptr && delNode->mleft == nullptr && delNode->mright == nullptr){
		mroot = nullptr;
		mcount = 0;
		mtotalSize = 0;
		return error("%s: the tree has only root node which need to delete", __func__);
	}

	// Construct a replaced list of parents and the node to delete itself.
	// This is done because this is an immutable data structure and
	// therefore all ancestors of the node that will be deleted, up to and
	// including the root, need to be replaced.
	// TODO simplify reduce copy times
	parentStack newParents;
	for(int64_t i = parents.Len(); i > 0; i--) {
		std::shared_ptr<treapNode> pnode = parents.At(i -1);
		pnode->msize--;
		newParents.Push(pnode);
	}
	delNode = newParents.Pop();
	std::shared_ptr<treapNode> parentOut = newParents.At(0);


	// Perform rotations to move the node to delete to a leaf position while
	// maintaining the min-heap while replacing the modified children.
	std::shared_ptr<treapNode> child = nullptr;
	std::shared_ptr<treapNode> newRoot = newParents.At(newParents.Len() - 1);
	while(delNode->mleft != nullptr || delNode->mright != nullptr){
		// Choose the child with the higher priority.
		bool isLeft = false;
		if(delNode->mleft == nullptr){
			child = delNode->mright;
		} else if(delNode->mright == nullptr){
			child = delNode->mleft;
			isLeft = true;
		} else if (delNode->mleft->mpriority <= delNode->mright->mpriority){
			child = delNode->mleft;
			isLeft = true;
		} else {
			child = delNode->mright;
		}
		// Rotate left or right depending on which side the child node
		// is on.  This has the effect of moving the node to delete
		// towards the bottom of the tree while maintaining the
		// min-heap.
		if(isLeft){
			child->msize += delNode->rightsize();
			delNode->mleft = child->mright;
			child->mright = delNode;
		} else {
			child->msize += delNode->leftsize();
			delNode->mright = child->mleft;
			child->mleft = delNode;
		}

		// Either set the new root of the tree when there is no
		// grandparent or relink the grandparent to the node based on
		// which side the old parent the node is replacing was on.
		//
		// Since the node to be deleted was just moved down a level, the
		// new grandparent is now the current parent and the new parent
		// is the current child.
		if(parentOut == nullptr){
			newRoot = child;
		} else if (parentOut->mleft == delNode){
			parentOut->mleft = child;
		} else {
			parentOut->mright = child;
		}

		// The parent for the node to delete is now what was previously
		// its child.
		parentOut = child;
	}

	// Delete the node, which is now a leaf node, by disconnecting it from
	// its parent.
	if(parentOut->mright == delNode){
		parentOut->mright = nullptr;
	} else {
		parentOut->mleft = nullptr;
	}
	delNode.reset();	// delete the treapNode

	mroot = newRoot;
	mcount--;
	mtotalSize -= delNode->nodeSize();
	return true;
}


// ForEach invokes the passed function with every key/value pair in the treap
// in ascending order.
void Immutable::ForEach(TicketHashes& hashes){
	// Add the root node and all children to the left of it to the list of
	// nodes to traverse and loop until they, and all of their child nodes,
	// have been traversed.
	parentStack parents;
	for(std::shared_ptr<treapNode> node = mroot; node != nullptr; node = node->mleft){
		parents.Push(node);
	}

	while(parents.Len() > 0){
		std::shared_ptr<treapNode> pnode = parents.Pop();

		hashes.push_back(pnode->mkey);

		// Extend the nodes to traverse by all children to the left of
		// the current node's right child.
		for(std::shared_ptr<treapNode> nodeIn = pnode->mright; nodeIn != nullptr; nodeIn = nodeIn->mleft){
			parents.Push(nodeIn);
		}
	}
}

// FetchWinnersAndExpired is a ticket database specific function which iterates
// over the entire treap and finds winners at selected indexes and all tickets
// whose height is less than or equal to the passed height. These are returned
// as slices of pointers to keys, which can be recast as []*chainhash.Hash.
// This is only used for benchmarking and is not consensus compatible.
void Immutable::FetchWinnersAndExpired(std::vector<uint32_t> idxs, uint32_t heightLine, std::vector<uint256*>& winners, std::vector<uint256*>& expired){
	size_t lengthIdxs = idxs.size();

	if(lengthIdxs == 0){
		return;
	}

	std::sort(idxs.begin(), idxs.end());	// TODO: IDX has been sorted

	// TODO buffer winners according to the TicketsPerBlock value from chaincfg?
	uint32_t idx = 0;
	uint32_t winnerIdx = 0;

	// Add the root node and all children to the left of it to the list of
	// nodes to traverse and loop until they, and all of their child nodes,
	// have been traversed.
	parentStack parents;
	for(std::shared_ptr<treapNode> node = mroot; node != nullptr; node = node->mleft){
		parents.Push(node);
	}
	while(parents.Len() > 0){
		std::shared_ptr<treapNode> pnode = parents.Pop();
		if(pnode->mheight <= heightLine){
			expired.push_back(&pnode->mkey);
		}
		if(idx == idxs[winnerIdx]){
			winners.push_back(&pnode->mkey);
			if(winnerIdx + 1 < lengthIdxs){
				winnerIdx++;
			}
		}
		idx++;

		// Extend the nodes to traverse by all children to the left of
		// the current node's right child.
		for(std::shared_ptr<treapNode> rnode = pnode->mright; rnode != nullptr; rnode = rnode->mleft){
			parents.Push(rnode);
		}
	}
}
