#include "txmempool.h"

int main (int argc, char *argv[]) {
    CFeeRate rate;
    CTxMemPool mempool(rate);
    CAutoFile est_filein(fopen(argv[1], "rb"), SER_DISK, CLIENT_VERSION);

    if (mempool.ReadFeeEstimates(est_filein)) {
        return 0;
    } else {
        return -1;
    }
}
