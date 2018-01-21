load(":hex_header.bzl", "json_hex_header", "raw_hex_header")

raw_hex_header(
    name = "alertTests",
    visibility = ["//src/test:__pkg__"],
    src = "alertTests.raw",
)

json_hex_header(
    name = "base58_encode_decode",
    visibility = ["//src/test:__pkg__"],
    src = "base58_encode_decode.json",
)

json_hex_header(
    name = "base58_keys_invalid",
    visibility = ["//src/test:__pkg__"],
    src = "base58_keys_invalid.json",
)

json_hex_header(
    name = "base58_keys_valid",
    visibility = ["//src/test:__pkg__"],
    src = "base58_keys_valid.json",
)

filegroup(
    name = "bitcoin_util_test_files",
    visibility = ["//visibility:public"],
    srcs = [
        "bitcoin-util-test.json",
        "blanktx.hex",
        "tt-delin1-out.hex",
        "tt-delout1-out.hex",
        "tt-locktime317000-out.hex",
        "tx394b54bb.hex",
        "txcreate1.hex",
        "txcreate2.hex",
        "txcreatesign.hex",
    ],
)

json_hex_header(
    name = "g1_compressed",
    visibility = ["//src/gtest:__pkg__"],
    src = "g1_compressed.json",
)

json_hex_header(
    name = "g2_compressed",
    visibility = ["//src/gtest:__pkg__"],
    src = "g2_compressed.json",
)

json_hex_header(
    name = "merkle_commitments",
    visibility = ["//src/gtest:__pkg__"],
    src = "merkle_commitments.json",
)

json_hex_header(
    name = "merkle_path",
    visibility = ["//src/gtest:__pkg__"],
    src = "merkle_path.json",
)

json_hex_header(
    name = "merkle_roots",
    visibility = ["//src/gtest:__pkg__"],
    src = "merkle_roots.json",
)

json_hex_header(
    name = "merkle_roots_empty",
    visibility = ["//src/gtest:__pkg__"],
    src = "merkle_roots_empty.json",
)

json_hex_header(
    name = "merkle_serialization",
    visibility = ["//src/gtest:__pkg__"],
    src = "merkle_serialization.json",
)

json_hex_header(
    name = "merkle_witness_serialization",
    visibility = ["//src/gtest:__pkg__"],
    src = "merkle_witness_serialization.json",
)

json_hex_header(
    name = "tx_invalid",
    visibility = ["//src/test:__pkg__"],
    src = "tx_invalid.json",
)

json_hex_header(
    name = "tx_valid",
    visibility = ["//src/test:__pkg__"],
    src = "tx_valid.json",
)

json_hex_header(
    name = "script_invalid",
    visibility = ["//src/test:__pkg__"],
    src = "script_invalid.json",
)

json_hex_header(
    name = "script_valid",
    visibility = ["//src/test:__pkg__"],
    src = "script_valid.json",
)

json_hex_header(
    name = "sighash",
    visibility = ["//src/test:__pkg__"],
    src = "sighash.json",
)
