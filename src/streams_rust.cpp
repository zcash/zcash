// Copyright (c) 2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "streams_rust.h"

rust::Box<stream::CppStream> ToRustStream(RustDataStream& stream) {
    return stream::from_data(stream);
}

rust::Box<stream::CppStream> ToRustStream(CAutoFile& file) {
    return stream::from_auto_file(file);
}

rust::Box<stream::CppStream> ToRustStream(CBufferedFile& file) {
    return stream::from_buffered_file(file);
}

rust::Box<stream::CppStream> ToRustStream(CHashWriter& writer) {
    return stream::from_hash_writer(writer);
}

rust::Box<stream::CppStream> ToRustStream(CBLAKE2bWriter& writer) {
    return stream::from_blake2b_writer(writer);
}

rust::Box<stream::CppStream> ToRustStream(CSizeComputer& sc) {
    return stream::from_size_computer(sc);
}
