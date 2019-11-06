bool fuzz_TxDeserializeFunction (const std::vector<unsigned char> txData) {
        CTransaction tx;
        CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
        try {
            ssData >> tx;
            return true;
        } catch (const std::exception&) {
            return false;
        }
}


int fuzz_TxDeserialize (int argc, char *argv[]) {
        std::ifstream t(argv[1]);
        std::vector<unsigned char> vec((std::istreambuf_iterator<char>(t)),
                                         std::istreambuf_iterator<char>());
        if (fuzz_TxDeserializeFunction (vec)) { fprintf(stdout, "Deserialized the transaction.") ; return 0; }
        else { fprintf(stderr, "Could not deserialize the transaction.") ; return -1; }
}

int main (int argc, char *argv[]) { return fuzz_TxDeserialize(argc, argv); }
