#ifndef ZCINCREMENTALMERKLETREE_H_
#define ZCINCREMENTALMERKLETREE_H_

#include <deque>
#include <boost/variant.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>

#include "uint256.h"
#include "serialize.h"
#include "zerocash/utils/util.h"

using namespace libzerocash;

namespace NewTree {

template <typename Hash>
class Parent;

template <size_t Depth, typename Hash>
class IncrementalWitness;

template <size_t Depth, typename Hash>
class IncrementalMerkleTree {
friend class IncrementalWitness<Depth, Hash>;

public:
    bool is_complete(size_t depth = Depth);
    size_t next_incomplete(size_t skip);

    BOOST_STATIC_ASSERT(Depth != 0);

    IncrementalMerkleTree() : parent(), left(boost::none), right(boost::none) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(left);
        READWRITE(right);
        READWRITE(parent);
    }

    void append(Hash obj);

    Hash root() const
    {
        return root(Depth);
    }

    IncrementalWitness<Depth, Hash> witness() const {
        return IncrementalWitness<Depth, Hash>(*this);
    }

private:
    boost::optional<Hash> left;
    boost::optional<Hash> right;
    Parent<Hash> parent;
    std::vector<bool> index() const;
    Hash root(size_t depth) const;
    Hash root(std::deque<Hash> fill, size_t depth = Depth) const;
    std::vector<Hash> path(std::deque<Hash> fill, size_t depth = Depth) const;
};

template <typename Hash>
class Parent {
template<size_t D, class H> friend class IncrementalMerkleTree;

private:
    Parent<Hash> *parent;
    boost::optional<Hash> left;

    ~Parent() {
        delete parent;
    }

    Parent(const Parent& other)
    {
        if (other.parent) {
            parent = new Parent(*other.parent);
        } else {
            parent = NULL;
        }
        left = other.left;
    }

    Parent& operator= (const Parent& other)
    {
        Parent tmp(other);
        *this = std::move(tmp);
        return *this;
    }

    Parent& operator= (Parent&& other) noexcept
    {
        delete parent;
        parent = other.parent;

        left = other.left;
        other.parent = nullptr;
        return *this;
    }

    Parent()
    {
        parent = NULL;
        left = boost::none;
    }

    void advance(Hash hash);
    
    template<typename Calculator>
    typename Calculator::Result ascend(size_t depth, Calculator& calc) const;

public:
    size_t GetSerializeSize(int nType, int nVersion) const {
        // Discriminant
        size_t size = 1;

        // Parent
        if (parent) {
            size += parent->GetSerializeSize(nType, nVersion);
        }

        size += left.GetSerializeSize(nType, nVersion);

        return size;
    }

    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        if (parent) {
            unsigned char discriminant = 0xff;
            ::Serialize(s, discriminant, nType, nVersion);
            parent->Serialize(s, nType, nVersion);
        } else {
            unsigned char discriminant = 0x00;
            ::Serialize(s, discriminant, nType, nVersion);
        }

        ::Serialize(s, left, nType, nVersion);
    }

    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        unsigned char discriminant = 0x00;
        ::Unserialize(s, discriminant, nType, nVersion);

        delete parent;

        if (discriminant == 0x00) {
            parent = NULL;
        } else {
            parent = new Parent();
            parent->Unserialize(s, nType, nVersion);
        }

        ::Unserialize(s, left, nType, nVersion);
    }
};

template <size_t Depth, typename Hash>
class IncrementalDelta {
friend class IncrementalWitness<Depth, Hash>;

private:
    std::vector<Hash> uncles;
    boost::optional<IncrementalMerkleTree<Depth, Hash>> active;
    uint32_t depth;
    IncrementalDelta() : uncles(), active(boost::none), depth(0) { }

public:
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(uncles);
        READWRITE(active);
        READWRITE(depth);
    }
};

class MerklePath {
public:
    std::vector<std::vector<bool>> authentication_path;
    std::vector<bool> index;

    MerklePath(std::vector<std::vector<bool>> authentication_path, std::vector<bool> index)
    : authentication_path(authentication_path), index(index) { }
};

template <size_t Depth, typename Hash>
class IncrementalWitness {
public:
    IncrementalMerkleTree<Depth, Hash> tree;
    IncrementalDelta<Depth, Hash> delta;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(tree);
        READWRITE(delta);
    }

    IncrementalWitness(IncrementalMerkleTree<Depth, Hash> tree) : tree(tree), delta() { }

    Hash root() const;
    MerklePath witness() const;
    void append(Hash obj);
};

class SHA256Compress : public uint256 {
public:
    SHA256Compress() : uint256() {}
    SHA256Compress(uint256 contents) : uint256(contents) { }

    static SHA256Compress combine(const SHA256Compress& a, const SHA256Compress& b);
};

}

#include "NewIncrementalMerkleTree.tcc"

#endif /* ZCINCREMENTALMERKLETREE_H_ */

