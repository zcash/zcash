#include "utilstrencodings.h"

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
  int64_t output = 0;
  ParseFixedPoint(s, 0, &output);
  return 0;
}

#endif
