#include "addrman.h"
#include "streams.h"


#ifdef FUZZ_WITH_AFL

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

#endif

#ifdef FUZZ_WITH_LIBFUZZER

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  CAddrMan addrman
  CDataStream ds(Data, Data+Size, 0, 0); // TODO: is this right for type and version? 
	try {
		ds >> addrman;
	} catch (const std::exception &e) {
		return 0;
  }
  return 0;  // Non-zero return values are reserved for future use.
}

#endif
