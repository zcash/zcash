// Copyright (c) 2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_STREAMS_RUST_H
#define ZCASH_STREAMS_RUST_H

#include "hash.h"
#include "serialize.h"
#include "streams.h"

#include <rust/bridge.h>
#include <rust/cxx.h>

rust::Box<stream::CppStream> ToRustStream(RustDataStream& stream);
rust::Box<stream::CppStream> ToRustStream(CAutoFile& file);
rust::Box<stream::CppStream> ToRustStream(CBufferedFile& file);
rust::Box<stream::CppStream> ToRustStream(CHashWriter& writer);
rust::Box<stream::CppStream> ToRustStream(CBLAKE2bWriter& writer);
rust::Box<stream::CppStream> ToRustStream(CSizeComputer& sc);

#endif // ZCASH_STREAMS_RUST_H
