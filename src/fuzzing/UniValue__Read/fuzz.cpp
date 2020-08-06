#include "univalue.h"

int fuzz_UniValue_Read(std::string notquitejson) {
  UniValue valRequest;
  valRequest.read(notquitejson);
  return 0;
}

#ifdef FUZZ_WITH_AFL

#error "The AFL version of this fuzzer has not yet been implemented."

int main (int argc, char *argv[]) {
    // not implemented
    return 0;
}

#endif // FUZZ_WITH_AFL
#ifdef FUZZ_WITH_LIBFUZZER

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  std::string s;
  s.assign((const char *)Data, Size);
  return fuzz_UniValue_Read(s);
}

#endif
