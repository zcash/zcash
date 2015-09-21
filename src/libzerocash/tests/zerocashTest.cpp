/** @file
 *****************************************************************************

 A test for Zerocash.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include <stdlib.h>
#include <iostream>

#include "timer.h"

#include "libzerocash/Zerocash.h"
#include "libzerocash/Address.h"
#include "libzerocash/CoinCommitment.h"
#include "libzerocash/Coin.h"
#include "libzerocash/IncrementalMerkleTree.h"
#include "libzerocash/MerkleTree.h"
#include "libzerocash/MintTransaction.h"
#include "libzerocash/PourTransaction.h"
#include "libzerocash/utils/util.h"

using namespace std;
using namespace libsnark;

vector<bool> convertIntToVector(uint64_t val) {
	vector<bool> ret;

	for(unsigned int i = 0; i < sizeof(val) * 8; ++i, val >>= 1) {
		ret.push_back(val & 0x01);
	}

	reverse(ret.begin(), ret.end());
	return ret;
}

int AddressTest() {
    cout << "\nADDRESS TEST\n" << endl;

    libzerocash::timer_start("Address");
    libzerocash::Address newAddress;
    libzerocash::timer_stop("Address");

    cout << "Successfully created an address.\n" << endl;

    CDataStream serializedAddress(SER_NETWORK, 7002);
    serializedAddress << newAddress;

    cout << "Successfully serialized an address.\n" << endl;

    libzerocash::Address addressNew;
    serializedAddress >> addressNew;
    cout << "Successfully deserialized an address.\n" << endl;

    libzerocash::PublicAddress pubAddress = newAddress.getPublicAddress();

    CDataStream serializedPubAddress(SER_NETWORK, 7002);
    serializedPubAddress << pubAddress;

    cout << "Successfully serialized a public address.\n" << endl;

    libzerocash::PublicAddress pubAddressNew;
    serializedPubAddress >> pubAddressNew;
    cout << "Successfully deserialized a public address.\n" << endl;

    bool result = ((newAddress == addressNew) && (pubAddress == pubAddressNew));

    return result;
}

int CoinTest() {
    cout << "\nCOIN TEST\n" << endl;

    libzerocash::Address newAddress;
    libzerocash::PublicAddress pubAddress = newAddress.getPublicAddress();

    libzerocash::Coin coin(pubAddress, 0);

    cout << "Successfully created a coin.\n" << endl;

    CDataStream serializedCoin(SER_NETWORK, 7002);
    serializedCoin << coin;

    cout << "Successfully serialized a coin.\n" << endl;

    libzerocash::Coin coinNew;
    serializedCoin >> coinNew;

    cout << "Successfully deserialized a coin.\n" << endl;

    ///////////////////////////////////////////////////////////////////////////

    libzerocash::timer_start("Coin");
    libzerocash::Coin coin2(pubAddress, 0);
    libzerocash::timer_stop("Coin");

    cout << "Successfully created a coin.\n" << endl;

    CDataStream serializedCoin2(SER_NETWORK, 7002);
    serializedCoin2 << coin2;

    cout << "Successfully serialized a coin.\n" << endl;

    libzerocash::Coin coinNew2;
    serializedCoin2 >> coinNew2;

    cout << "Successfully deserialized a coin.\n" << endl;

    bool result = ((coin == coinNew) && (coin2 == coinNew2));

    return result;
}

bool MintTxTest() {
    cout << "\nMINT TRANSACTION TEST\n" << endl;

    libzerocash::Address newAddress;
    libzerocash::PublicAddress pubAddress = newAddress.getPublicAddress();

    vector<unsigned char> value(v_size, 0);

    libzerocash::timer_start("Coin");
    const libzerocash::Coin coin(pubAddress, 0);
    libzerocash::timer_stop("Coin");

    libzerocash::timer_start("Mint Transaction");
    libzerocash::MintTransaction minttx(coin);
    libzerocash::timer_stop("Mint Transaction");

    cout << "Successfully created a mint transaction.\n" << endl;

    CDataStream serializedMintTx(SER_NETWORK, 7002);
    serializedMintTx << minttx;

    cout << "Successfully serialized a mint transaction.\n" << endl;

    libzerocash::MintTransaction minttxNew;

    serializedMintTx >> minttxNew;

    cout << "Successfully deserialized a mint transaction.\n" << endl;

    libzerocash::timer_start("Mint Transaction Verify");
    bool minttx_res = minttxNew.verify();
    libzerocash::timer_stop("Mint Transaction Verify");

    return minttx_res;
}

bool PourTxTest(const size_t tree_depth) {
    cout << "\nPOUR TRANSACTION TEST\n" << endl;

    cout << "Creating Params...\n" << endl;

    libzerocash::ZerocashParams p(tree_depth);

    cout << "Successfully created Params.\n" << endl;

    vector<libzerocash::Coin> coins(5);
    vector<libzerocash::Address> addrs(5);

    for(size_t i = 0; i < coins.size(); i++) {
        addrs.at(i) = libzerocash::Address();
        coins.at(i) = libzerocash::Coin(addrs.at(i).getPublicAddress(), i);
    }

    cout << "Successfully created coins.\n" << endl;

    vector<std::vector<bool>> coinValues(5);

    vector<bool> temp_comVal(cm_size * 8);
    for(size_t i = 0; i < coinValues.size(); i++) {
        libzerocash::convertBytesVectorToVector(coins.at(i).getCoinCommitment().getCommitmentValue(), temp_comVal);
        coinValues.at(i) = temp_comVal;
        libzerocash::printVectorAsHex("Coin => ", coinValues.at(i));
    }

    cout << "Creating Merkle Tree...\n" << endl;

    libzerocash::timer_start("Merkle Tree");
    libzerocash::IncrementalMerkleTree merkleTree(coinValues, tree_depth);
    libzerocash::timer_stop("Merkle Tree");

    cout << "Successfully created Merkle Tree.\n" << endl;

    merkle_authentication_path witness_1(tree_depth);

    libzerocash::timer_start("Witness");
    if (merkleTree.getWitness(convertIntToVector(1), witness_1) == false) {
		cout << "Could not get witness" << endl;
		return false;
	}
    libzerocash::timer_stop("Witness");

    cout << "Witness 1: " << endl;
    for(size_t i = 0; i < witness_1.size(); i++) {
        libzerocash::printVectorAsHex(witness_1.at(i));
    }
    cout << "\n" << endl;

    merkle_authentication_path witness_2(tree_depth);
    if (merkleTree.getWitness(convertIntToVector(3), witness_2) == false) {
		cout << "Could not get witness" << endl;
	}

    cout << "Witness 2: " << endl;
    for(size_t i = 0; i < witness_2.size(); i++) {
        libzerocash::printVectorAsHex(witness_2.at(i));
    }
    cout << "\n" << endl;

    libzerocash::Address newAddress3;
    libzerocash::PublicAddress pubAddress3 = newAddress3.getPublicAddress();

    libzerocash::Address newAddress4;
    libzerocash::PublicAddress pubAddress4 = newAddress4.getPublicAddress();

    libzerocash::Coin c_1_new(pubAddress3, 2);
    libzerocash::Coin c_2_new(pubAddress4, 2);

    vector<bool> root_bv(root_size * 8);
    merkleTree.getRootValue(root_bv);
    vector<unsigned char> rt(root_size);
    libzerocash::convertVectorToBytesVector(root_bv, rt);

    vector<unsigned char> ones(v_size, 1);
    vector<unsigned char> twos(v_size, 2);
    vector<unsigned char> as(sig_pk_size, 'a');

    cout << "Creating a pour transaction...\n" << endl;

    libzerocash::timer_start("Pour Transaction");
    libzerocash::PourTransaction pourtx(1, p, rt, coins.at(1), coins.at(3), addrs.at(1), addrs.at(3), 1, 3, witness_1, witness_2, pubAddress3, pubAddress4, 0, as, c_1_new, c_2_new);
    libzerocash::timer_stop("Pour Transaction");

    cout << "Successfully created a pour transaction.\n" << endl;

    CDataStream serializedPourTx(SER_NETWORK, 7002);
    serializedPourTx << pourtx;

    cout << "Successfully serialized a pour transaction.\n" << endl;

    libzerocash::PourTransaction pourtxNew;
    serializedPourTx >> pourtxNew;

    cout << "Successfully deserialized a pour transaction.\n" << endl;

	std::vector<unsigned char> pubkeyHash(sig_pk_size, 'a');

    libzerocash::timer_start("Pour Transaction Verify");
    bool pourtx_res = pourtxNew.verify(p, pubkeyHash, rt);
    libzerocash::timer_stop("Pour Transaction Verify");

    return pourtx_res;
}

bool MerkleTreeSimpleTest() {
    cout << "\nMERKLE TREE SIMPLE TEST\n" << endl;

    vector<libzerocash::Coin> coins(5);
    vector<libzerocash::Address> addrs(5);

    cout << "Creating coins...\n" << endl;

    for(size_t i = 0; i < coins.size(); i++) {
        addrs.at(i) = libzerocash::Address();
        coins.at(i) = libzerocash::Coin(addrs.at(i).getPublicAddress(), i);
    }

    cout << "Successfully created coins.\n" << endl;

    vector<std::vector<bool>> coinValues(coins.size());

    vector<bool> temp_comVal(cm_size * 8);
    for(size_t i = 0; i < coinValues.size(); i++) {
        libzerocash::convertBytesVectorToVector(coins.at(i).getCoinCommitment().getCommitmentValue(), temp_comVal);
        coinValues.at(i) = temp_comVal;
        libzerocash::printVectorAsHex(coinValues.at(i));
    }

    cout << "Creating Merkle Tree...\n" << endl;

    libzerocash::IncrementalMerkleTree merkleTree(64);
	vector<bool> root;
	merkleTree.getRootValue(root);
	cout << "Root: ";
	libzerocash::printVectorAsHex(root);
	cout << endl;

	libzerocash::MerkleTree christinaTree(coinValues, 16);
	christinaTree.getRootValue(root);
	cout << "Christina root: ";
	libzerocash::printVectorAsHex(root);
	cout << endl;

    cout << "Successfully created Merkle Tree.\n" << endl;

	cout << "Copying and pruning Merkle Tree...\n" << endl;
	libzerocash::IncrementalMerkleTree copyTree = merkleTree;
	copyTree.prune();

	cout << "Obtaining compact representation and reconstituting tree...\n" << endl;
	libzerocash::IncrementalMerkleTreeCompact compactTree;
	merkleTree.getCompactRepresentation(compactTree);

	cout << "Compact representation vector: ";
	libzerocash::printBytesVector(compactTree.hashListBytes);
	libzerocash::printVector(compactTree.hashList);

	libzerocash::IncrementalMerkleTree reconstitutedTree(compactTree);
	reconstitutedTree.getRootValue(root);
	cout << "New root: ";
	libzerocash::printVectorAsHex(root);
	cout << endl;

	reconstitutedTree.insertVector(coinValues);
	merkleTree.insertVector(coinValues);

	vector<bool> index;
	reconstitutedTree.getRootValue(root);
	cout << "New root (added a bunch more): ";
	libzerocash::printVectorAsHex(root);
	cout << endl;

	merkleTree.getRootValue(root);
	cout << "Old root (added a bunch more): ";
	libzerocash::printVectorAsHex(root);
	cout << endl;

	vector<bool> testVec = convertIntToVector(1);
	libzerocash::printVector(testVec);

    merkle_authentication_path witness(3);
	if (merkleTree.getWitness(convertIntToVector(1), witness) == false) {
		cout << "Witness generation failed." << endl;
		return false;
	}

    cout << "Successfully created witness.\n" << endl;

    cout << "Witness: " << endl;
    for(size_t i = 0; i < witness.size(); i++) {
        libzerocash::printVectorAsHex(witness.at(i));
    }
    cout << "\n" << endl;

	christinaTree.getWitness(coinValues.at(1), witness);

    cout << "Christina created witness.\n" << endl;

    cout << "Witness: " << endl;
    for(size_t i = 0; i < witness.size(); i++) {
        libzerocash::printVectorAsHex(witness.at(i));
    }
    cout << "\n" << endl;

    vector<bool> wit1(SHA256_BLOCK_SIZE * 8);
    vector<bool> wit2(SHA256_BLOCK_SIZE * 8);
    vector<bool> wit3(SHA256_BLOCK_SIZE * 8);
    vector<bool> inter_1(SHA256_BLOCK_SIZE * 8);
    vector<bool> inter_2(SHA256_BLOCK_SIZE * 8);

    wit1 = coinValues.at(2);
    libzerocash::hashVectors(coinValues.at(0), coinValues.at(1), wit2);
    libzerocash::hashVectors(coinValues.at(4), vector<bool>(SHA256_BLOCK_SIZE * 8, 0), inter_1);
    libzerocash::hashVectors(vector<bool>(SHA256_BLOCK_SIZE * 8, 0), vector<bool>(SHA256_BLOCK_SIZE * 8, 0), inter_2);
    libzerocash::hashVectors(inter_1, inter_2, wit3);

    return ((witness.at(0) == wit3) && (witness.at(1) == wit2) && (witness.at(2) == wit1));
}

bool SimpleTxTest(const size_t tree_depth) {
    cout << "\nSIMPLE TRANSACTION TEST\n" << endl;

    cout << "Creating Params...\n" << endl;
    libzerocash::ZerocashParams p(tree_depth);
    cout << "Successfully created Params.\n" << endl;

    vector<libzerocash::Coin> coins(5);
    vector<libzerocash::Address> addrs(5);

    cout << "Creating Addresses and Coins...\n" << endl;
    for(size_t i = 0; i < coins.size(); i++) {
        addrs.at(i) = libzerocash::Address();
        coins.at(i) = libzerocash::Coin(addrs.at(i).getPublicAddress(), i);
    }
    cout << "Successfully created address and coins.\n" << endl;

    cout << "Creating a Mint Transaction...\n" << endl;
    libzerocash::MintTransaction minttx(coins.at(0));
    cout << "Successfully created a Mint Transaction.\n" << endl;

    cout << "Serializing a mint transaction...\n" << endl;
    CDataStream serializedMintTx(SER_NETWORK, 7002);
    serializedMintTx << minttx;
    cout << "Successfully serialized a mint transaction.\n" << endl;

    libzerocash::MintTransaction minttxNew;
    serializedMintTx >> minttxNew;
    cout << "Successfully deserialized a mint transaction.\n" << endl;

    cout << "Verifying a Mint Transaction...\n" << endl;
    bool minttx_res = minttxNew.verify();

    vector<std::vector<bool>> coinValues(5);
    vector<bool> temp_comVal(cm_size * 8);
    for(size_t i = 0; i < coinValues.size(); i++) {
        libzerocash::convertBytesVectorToVector(coins.at(i).getCoinCommitment().getCommitmentValue(), temp_comVal);
        coinValues.at(i) = temp_comVal;
    }

    cout << "Creating Merkle Tree...\n" << endl;
    libzerocash::IncrementalMerkleTree merkleTree(coinValues, tree_depth);
    cout << "Successfully created Merkle Tree.\n" << endl;

    cout << "Creating Witness 1...\n" << endl;
    merkle_authentication_path witness_1(tree_depth);
    if (merkleTree.getWitness(convertIntToVector(1), witness_1) == false) {
		cout << "Could not get witness" << endl;
		return false;
	}
    cout << "Successfully created Witness 1.\n" << endl;

    cout << "Creating Witness 2...\n" << endl;
    merkle_authentication_path witness_2(tree_depth);
    if (merkleTree.getWitness(convertIntToVector(3), witness_2) == false) {
		cout << "Could not get witness" << endl;
	}
    cout << "Successfully created Witness 2.\n" << endl;

    cout << "Creating coins to spend...\n" << endl;
    libzerocash::Address newAddress3;
    libzerocash::PublicAddress pubAddress3 = newAddress3.getPublicAddress();

    libzerocash::Address newAddress4;
    libzerocash::PublicAddress pubAddress4 = newAddress4.getPublicAddress();

    libzerocash::Coin c_1_new(pubAddress3, 2);
    libzerocash::Coin c_2_new(pubAddress4, 2);
    cout << "Successfully created coins to spend.\n" << endl;

    vector<bool> root_bv(root_size * 8);
    merkleTree.getRootValue(root_bv);
    vector<unsigned char> rt(root_size);
    libzerocash::convertVectorToBytesVector(root_bv, rt);


    vector<unsigned char> as(sig_pk_size, 'a');

    cout << "Creating a pour transaction...\n" << endl;
    libzerocash::PourTransaction pourtx(1, p,
    		rt,
    		coins.at(1), coins.at(3),
    		addrs.at(1), addrs.at(3),
                1, 3,
                witness_1, witness_2,
    		pubAddress3, pubAddress4,
    		0,
    		as,
    		c_1_new, c_2_new);
    cout << "Successfully created a pour transaction.\n" << endl;

    cout << "Serializing a pour transaction...\n" << endl;
    CDataStream serializedPourTx(SER_NETWORK, 7002);
    serializedPourTx << pourtx;
    cout << "Successfully serialized a pour transaction.\n" << endl;

    libzerocash::PourTransaction pourtxNew;
    serializedPourTx >> pourtxNew;
    cout << "Successfully deserialized a pour transaction.\n" << endl;

	std::vector<unsigned char> pubkeyHash(sig_pk_size, 'a');

    cout << "Verifying a pour transaction...\n" << endl;
    bool pourtx_res = pourtxNew.verify(p, pubkeyHash, rt);

    return (minttx_res && pourtx_res);
}

bool ParamFilesTest(const size_t tree_depth, string pkFile, string vkFile) {
    cout << "\nPARAM FILES TEST\n" << endl;

    cout << "Creating Params...\n" << endl;
    libzerocash::ZerocashParams p(tree_depth, pkFile, vkFile);
    cout << "Successfully created Params.\n" << endl;

    vector<libzerocash::Coin> coins(5);
    vector<libzerocash::Address> addrs(5);

    cout << "Creating Addresses and Coins...\n" << endl;
    for(size_t i = 0; i < coins.size(); i++) {
        addrs.at(i) = libzerocash::Address();
        coins.at(i) = libzerocash::Coin(addrs.at(i).getPublicAddress(), i);
    }
    cout << "Successfully created address and coins.\n" << endl;

    cout << "Creating a Mint Transaction...\n" << endl;
    libzerocash::MintTransaction minttx(coins.at(0));
    cout << "Successfully created a Mint Transaction.\n" << endl;

    cout << "Serializing a mint transaction...\n" << endl;
    CDataStream serializedMintTx(SER_NETWORK, 7002);
    serializedMintTx << minttx;
    cout << "Successfully serialized a mint transaction.\n" << endl;

    libzerocash::MintTransaction minttxNew;
    serializedMintTx >> minttxNew;
    cout << "Successfully deserialized a mint transaction.\n" << endl;

    cout << "Verifying a Mint Transaction...\n" << endl;
    bool minttx_res = minttxNew.verify();

    vector<std::vector<bool>> coinValues(5);
    vector<bool> temp_comVal(cm_size * 8);
    for(size_t i = 0; i < coinValues.size(); i++) {
        libzerocash::convertBytesVectorToVector(coins.at(i).getCoinCommitment().getCommitmentValue(), temp_comVal);
        coinValues.at(i) = temp_comVal;
    }

    cout << "Creating Merkle Tree...\n" << endl;
    libzerocash::MerkleTree merkleTree(coinValues, tree_depth);
    cout << "Successfully created Merkle Tree.\n" << endl;

    cout << "Creating Witness 1...\n" << endl;
    merkle_authentication_path witness_1(tree_depth);
    merkleTree.getWitness(coinValues.at(1), witness_1);
    cout << "Successfully created Witness 1.\n" << endl;

    cout << "Creating Witness 2...\n" << endl;
    merkle_authentication_path witness_2(tree_depth);
    merkleTree.getWitness(coinValues.at(3), witness_2);
    cout << "Successfully created Witness 2.\n" << endl;

    cout << "Creating coins to spend...\n" << endl;
    libzerocash::Address newAddress3;
    libzerocash::PublicAddress pubAddress3 = newAddress3.getPublicAddress();

    libzerocash::Address newAddress4;
    libzerocash::PublicAddress pubAddress4 = newAddress4.getPublicAddress();

    libzerocash::Coin c_1_new(pubAddress3, 2);
    libzerocash::Coin c_2_new(pubAddress4, 2);
    cout << "Successfully created coins to spend.\n" << endl;

    vector<bool> root_bv(root_size * 8);
    merkleTree.getRootValue(root_bv);
    vector<unsigned char> rt(root_size);
    libzerocash::convertVectorToBytesVector(root_bv, rt);


    vector<unsigned char> as(sig_pk_size, 'a');

    cout << "Creating a pour transaction...\n" << endl;
    libzerocash::PourTransaction pourtx(1, p,
    		rt,
    		coins.at(1), coins.at(3),
    		addrs.at(1), addrs.at(3),
                1, 3,
    		witness_1, witness_2,
    		pubAddress3, pubAddress4,
    		0,
    		as,
    		c_1_new, c_2_new);
    cout << "Successfully created a pour transaction.\n" << endl;

    cout << "Serializing a pour transaction...\n" << endl;
    CDataStream serializedPourTx(SER_NETWORK, 7002);
    serializedPourTx << pourtx;
    cout << "Successfully serialized a pour transaction.\n" << endl;

    libzerocash::PourTransaction pourtxNew;
    serializedPourTx >> pourtxNew;
    cout << "Successfully deserialized a pour transaction.\n" << endl;

	std::vector<unsigned char> pubkeyHash(sig_pk_size, 'a');

    cout << "Verifying a pour transaction...\n" << endl;
    bool pourtx_res = pourtxNew.verify(p, pubkeyHash, rt);

    return (minttx_res && pourtx_res);
}

int main(int argc, char **argv)
{
	cout << "libzerocash v" << ZEROCASH_VERSION_STRING << " test." << endl << endl;

    const size_t tree_depth = 4;

	bool merkleSimpleResult = MerkleTreeSimpleTest();
    bool addressResult = AddressTest();
    bool coinResult = CoinTest();
    bool mintTxResult = MintTxTest();

    bool pourTxResult = PourTxTest(tree_depth);
    bool simpleTxResult = SimpleTxTest(tree_depth);

    cout << "\n" << endl;
    std::cout << "\nAddressTest result => " << addressResult << std::endl;
    std::cout << "\nCoinTest result => " << coinResult << std::endl;
    std::cout << "\nMerkleTreeSimpleTest result => " << merkleSimpleResult << std::endl;
    std::cout << "\nMintTxTest result => " << mintTxResult << std::endl;
    std::cout << "\nPourTxTest result => " << pourTxResult << std::endl;
    std::cout << "\nSimpleTxTest result => " << simpleTxResult << std::endl;
}
