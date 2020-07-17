#include "addrman.h"
#include "streams.h"

int main (int argc, char *argv[]) {
    CAddrMan addrman;
    CAutoFile filein(fopen(argv[1], "rb"), SER_DISK, CLIENT_VERSION);
    try {
        filein >> addrman;
        return 0;
    } catch (const std::exception&) {
        return -1;
    }
}
