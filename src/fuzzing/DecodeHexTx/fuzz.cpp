#include<bits/stdc++.h>
#include <string>

extern bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);

// actual fuzzer

bool fuzz_DecodeHexTxFunction (const std::string& strHexTx) {
        CTransaction tx;
        return DecodeHexTx(tx, strHexTx);
}

#ifdef FUZZ_WITH_AFL

// AFL

int fuzz_DecodeHexTx (int argc, char *argv[]) {
        std::ifstream t(argv[1]);
        std::string str((std::istreambuf_iterator<char>(t)),
                                         std::istreambuf_iterator<char>());
        if (fuzz_DecodeHexTxFunction (str)) { fprintf(stdout, "Decoded hex string") ; return 0; }
        else { fprintf(stderr, "Could not decode hex string") ; return -1; }
}

int main (int argc, char *argv[]) { return fuzz_DecodeHexTx(argc, argv); }

#endif
#ifdef FUZZ_WITH_LIBFUZZER

// libfuzzer

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  std::string s;
  s.assign((const char *)Data, Size);
  fuzz_DecodeHexTxFunction (s);
  return 0;  // Non-zero return values are reserved for future use.
}

#endif
