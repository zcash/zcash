// Copyright (c) 2021 The Zcash Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ADDRESS_ORCHARD_H
#define ZCASH_ADDRESS_ORCHARD_H

#include "streams.h"
#include "rust/orchard/keys.h"

namespace libzcash {

class OrchardRawAddress
{
private:
public: //FIXME
    std::unique_ptr<OrchardRawAddressPtr, decltype(&orchard_address_free)> inner;

    OrchardRawAddress() : inner(nullptr, orchard_address_free) {}

    OrchardRawAddress(OrchardRawAddressPtr* ptr) : inner(ptr, orchard_address_free) {}

    OrchardRawAddress(OrchardRawAddress&& key) : inner(std::move(key.inner)) {}

    OrchardRawAddress(const OrchardRawAddress& key) :
        inner(orchard_address_clone(key.inner.get()), orchard_address_free) {}

    OrchardRawAddress& operator=(OrchardRawAddress&& key)
    {
        if (this != &key) {
            inner = std::move(key.inner);
        }
        return *this;
    }

    OrchardRawAddress& operator=(const OrchardRawAddress& key)
    {
        if (this != &key) {
            inner.reset(orchard_address_clone(key.inner.get()));
        }
        return *this;
    }

    friend bool operator==(const OrchardRawAddress& a0, const OrchardRawAddress& a1)
    {
        // FIXME
        return true;
    }

    friend bool operator<(const OrchardRawAddress& a0, const OrchardRawAddress& a1)
    {
        // FIXME
        return true;
    }

    template<typename Stream>
    void Serialize(Stream& s) const {
        RustStream rs(s);
        if (!orchard_address_serialize(inner.get(), &rs, RustStream<Stream>::write_callback)) {
            throw std::ios_base::failure("Failed to serialize Orchard incoming viewing key");
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        RustStream rs(s);
        OrchardRawAddressPtr* key;
        if (!orchard_address_parse(&rs, RustStream<Stream>::read_callback, &key)) {
            throw std::ios_base::failure("Failed to parse Orchard incoming viewing key");
        }
        inner.reset(key);
    }
};


class OrchardIncomingViewingKey
{
private:
    std::unique_ptr<OrchardIncomingViewingKeyPtr, decltype(&orchard_incoming_viewing_key_free)> inner;
public:
    OrchardIncomingViewingKey() : inner(nullptr, orchard_incoming_viewing_key_free) {}

    OrchardIncomingViewingKey(OrchardIncomingViewingKey&& key) : inner(std::move(key.inner)) {}

    OrchardIncomingViewingKey(const OrchardIncomingViewingKey& key) :
        inner(orchard_incoming_viewing_key_clone(key.inner.get()), orchard_incoming_viewing_key_free) {}

    OrchardIncomingViewingKey& operator=(OrchardIncomingViewingKey&& key)
    {
        if (this != &key) {
            inner = std::move(key.inner);
        }
        return *this;
    }

    OrchardIncomingViewingKey& operator=(const OrchardIncomingViewingKey& key)
    {
        if (this != &key) {
            inner.reset(orchard_incoming_viewing_key_clone(key.inner.get()));
        }
        return *this;
    }

    template<typename Stream>
    void Serialize(Stream& s) const {
        RustStream rs(s);
        if (!orchard_incoming_viewing_key_serialize(inner.get(), &rs, RustStream<Stream>::write_callback)) {
            throw std::ios_base::failure("Failed to serialize Orchard incoming viewing key");
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        RustStream rs(s);
        OrchardIncomingViewingKeyPtr* key;
        if (!orchard_incoming_viewing_key_parse(&rs, RustStream<Stream>::read_callback, &key)) {
            throw std::ios_base::failure("Failed to parse Orchard incoming viewing key");
        }
        inner.reset(key);
    }

};

class OrchardFullViewingKey
{
private:
    std::unique_ptr<OrchardFullViewingKeyPtr, decltype(&orchard_full_viewing_key_free)> inner;
public:
    OrchardFullViewingKey() : inner(nullptr, orchard_full_viewing_key_free) {}

    OrchardFullViewingKey(OrchardFullViewingKey&& key) : inner(std::move(key.inner)) {}

    OrchardFullViewingKey(const OrchardFullViewingKey& key) :
        inner(orchard_full_viewing_key_clone(key.inner.get()), orchard_full_viewing_key_free) {}

    OrchardFullViewingKey& operator=(OrchardFullViewingKey&& key)
    {
        if (this != &key) {
            inner = std::move(key.inner);
        }
        return *this;
    }

    OrchardFullViewingKey& operator=(const OrchardFullViewingKey& key)
    {
        if (this != &key) {
            inner.reset(orchard_full_viewing_key_clone(key.inner.get()));
        }
        return *this;
    }

    template<typename Stream>
    void Serialize(Stream& s) const {
        RustStream rs(s);
        if (!orchard_full_viewing_key_serialize(inner.get(), &rs, RustStream<Stream>::write_callback)) {
            throw std::ios_base::failure("Failed to serialize Orchard full viewing key");
        }
    }

    template<typename Stream>
    void Unserialize(Stream& s) {
        RustStream rs(s);
        OrchardFullViewingKeyPtr* key;
        if (!orchard_full_viewing_key_parse(&rs, RustStream<Stream>::read_callback, &key)) {
            throw std::ios_base::failure("Failed to parse Orchard full viewing key");
        }
        inner.reset(key);
    }
};

} // namespace libzcash

#endif // ZCASH_ADDRESS_ORCHARD_H

