#ifndef UINT252_H
#define UINT252_H

#include <vector>
#include "uint256.h"
#include "serialize.h"

// Wrapper of uint256 with guarantee that first
// four bits are zero.
class uint252 {
private:
    uint256 contents;

public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(contents);

        if ((*contents.begin()) & 0xF0) {
            throw std::ios_base::failure("spending key has invalid leading bits");
        }
    }

    const unsigned char* begin() const
    {
        return contents.begin();
    }

    const unsigned char* end() const
    {
        return contents.end();
    }

    uint252() : contents() {};
    explicit uint252(const uint256& in) : contents(in) {
        if (*contents.begin() & 0xF0) {
            throw std::domain_error("leading bits are set in argument given to uint252 constructor");
        }
    }

    uint256 inner() const {
        return contents;
    }

    friend inline bool operator==(const uint252& a, const uint252& b) { return a.contents == b.contents; }
};

#endif
