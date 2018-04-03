#ifndef KOMODO_TXPROOF_H
#define KOMODO_TXPROOF_H


class MoMProof
{
public:
    int nIndex;
    std::vector<uint256> branch;
    uint256 notarisationHash;

    MoMProof() {}
    MoMProof(int i, std::vector<uint256> b, uint256 n) : notarisationHash(n), nIndex(i), branch(b) {}

    ADD_SERIALIZE_METHODS;
    
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(VARINT(nIndex));
        READWRITE(branch);
        READWRITE(notarisationHash);
    }
};


#endif /* KOMODO_TXPROOF_H */
