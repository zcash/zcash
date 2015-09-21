/** @file
 *****************************************************************************

 Test for Merkle tree.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "libzerocash/IncrementalMerkleTree.h"
#include "libzerocash/MerkleTree.h"

#include <iostream>
#include <vector>

using namespace libzerocash;
using namespace std;

void writeOut(const vector<bool>& data)
{
  for (vector<bool>::const_iterator it = data.begin(); it != data.end(); it++)
  {
    cout << *it;
  }
}

void constructTestVector(std::vector< std::vector<bool> > &values)
{
	std::vector<bool> dummy;
	dummy.resize(256);

	for (int i = 0; i < 2; i++)
    {
		values.push_back(dummy);
	}
}

bool testCompactRep(IncrementalMerkleTree &inTree)
{
	IncrementalMerkleTreeCompact compact;
	std::vector<bool> root1, root2;

	if (inTree.getCompactRepresentation(compact) == false)
    {
		cout << "Unable to generate compact representation." << endl;
		return false;
	}

	IncrementalMerkleTree newTree(compact);

	inTree.getRootValue(root1);
	newTree.getRootValue(root2);

	cout << "Original root: ";
	printVector(root1);
	cout << endl;

	cout << "New root: ";
	printVector(root2);
	cout << endl;

	if (root1 == root2) {
		cout << "TEST PASSED" << endl;
	}

	return true;
}



int main()
{
  IncrementalMerkleTree incTree;

  std::vector<bool> index;
  std::vector< std::vector<bool> > values;
  std::vector<bool> rootHash;

  constructTestVector(values);

  if (incTree.insertVector(values) == false) {
	 cout << "Could not insert" << endl;
  }
  incTree.prune();

  MerkleTree oldTree(values);

  cout << "Incremental Tree: ";
  incTree.getRootValue(rootHash);
  writeOut(rootHash);
  cout << endl;

  cout << "Old Merkle Tree: ";
  oldTree.getRootValue(rootHash);
  writeOut(rootHash);
  cout << endl;

  testCompactRep(incTree);
}
