extern bool DecodeHexTx(CTransaction& tx, const std::string& strHexTx);

bool fuzz_DecodeHexTxFunction (const std::string& strHexTx) {
        CTransaction tx;
        return DecodeHexTx(tx, strHexTx);
}


int fuzz_DecodeHexTx (int argc, char *argv[]) {
        std::ifstream t(argv[1]);
        std::string str((std::istreambuf_iterator<char>(t)),
                                         std::istreambuf_iterator<char>());
        if (fuzz_DecodeHexTxFunction (str)) { fprintf(stdout, "Decoded hex string") ; return 0; }
        else { fprintf(stderr, "Could not decode hex string") ; return -1; }
}

int main (int argc, char *argv[]) { return fuzz_DecodeHexTx(argc, argv); }
