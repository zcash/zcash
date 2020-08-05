#ifdef FUZZ_WITH_AFL

#error "The AFL version of this fuzzer has not yet been implemented."

int main (int argc, char *argv[]) {
    // not implemented
    return 0;
}

#endif // FUZZ_WITH_AFL
#ifdef FUZZ_WITH_LIBFUZZER

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size == 4) {
      if (data[0] == 'c') {
          if (data[1] == 'r') {
              if (data[2] == 's') {
                  if (data[3] == 'h') {
                        int k = 0x7fffffff;
                        k += 1; // will overflow
                        return 0;
                  }
              }
          }
      }
  }
  return 0;
}

#endif
