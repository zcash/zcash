#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "config/bitcoin-config.h"
#include "chainparams.h"
#include "base58.h"

int main(int argc, char **argv)
{
	if(argc != 2) {
		fprintf(stderr, "need seed as an argument\n");
		return 1;
	}
	if(strlen(argv[1]) == 64) {
		fprintf(stderr, "WARNING: seed should be 63, not 64 characters. Ignoring the first.\n");
		argv[1]++;
	}
	if(strlen(argv[1]) != 63) {
		fprintf(stderr, "seed must be exactly 63 characters\n");
		return 1;
	}

	std::vector<unsigned char> vec;
	char digit[3];
	digit[2] = '\x0';
	for(int i=0; i<32; i++) {
		if(i == 0) {
			digit[0] = argv[1][0];
			digit[1] = '\x0';
		} else {
			digit[0] = argv[1][i*2-1];
			digit[1] = argv[1][i*2];
		}
		long int n = strtol(digit, NULL, 16);
		unsigned char chr = n;
		vec.push_back(chr);
	}
	uint256 uv(vec);
	uint252 uv2(uv);

	SelectParams(CBaseChainParams::MAIN);
	libzcash::SpendingKey k(uv2);
	CZCSpendingKey ck(k);
	printf("sk: %s\n", ck.ToString().c_str());
	CZCPaymentAddress pa = k.address();
	printf("pa: %s\n", pa.ToString().c_str());

	return 0;
}
