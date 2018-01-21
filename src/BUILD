cc_library(
    name = "addrman",
    visibility = ["//visibility:public"],
    srcs = ["addrman.cpp"],
    hdrs = ["addrman.h"],
    deps = [
        ":hash",
        ":netbase",
        ":protocol",
        ":serialize",
        ":streams",
        ":timedata",
        ":util",
    ],
)

cc_library(
    name = "alertkeys",
    visibility = ["//visibility:public"],
    hdrs = ["alertkeys.h"],
)

cc_library(
    name = "amount",
    visibility = ["//visibility:public"],
    srcs = ["amount.cpp"],
    hdrs = ["amount.h"],
    deps = [
        ":serialize",
        ":tinyformat",
    ],
)

# TODO(per-gron): This library ought to be declared in a BUILD file in the
# amqp directory but it's not because there are files there that are part
# of the big cyclic tangle, and Bazel does not allow having a BUILD file
# there when those files are used by libraries in this file.
cc_library(
    name = "amqp_amqpabstractnotifier",
    visibility = ["//visibility:public"],
    srcs = ["amqp/amqpabstractnotifier.cpp"],
    hdrs = ["amqp/amqpabstractnotifier.h"],
    deps = [
        ":amqp_amqpconfig",
        ":util",
    ],
)

# TODO(per-gron): This library ought to be declared in a BUILD file in the
# amqp directory but it's not because there are files there that are part
# of the big cyclic tangle, and Bazel does not allow having a BUILD file
# there when those files are used by libraries in this file.
cc_library(
    name = "amqp_amqpconfig",
    visibility = ["//visibility:public"],
    hdrs = ["amqp/amqpconfig.h"],
    deps = [
        "//src/config:bitcoin-config",
        "//src/primitives:block",
        "//src/primitives:transaction",
        "@proton//:qpid-proton-cpp",
    ],
)

# TODO(per-gron): This library ought to be declared in a BUILD file in the
# amqp directory but it's not because there are files there that are part
# of the big cyclic tangle, and Bazel does not allow having a BUILD file
# there when those files are used by libraries in this file.
cc_library(
    name = "amqp_amqpsender",
    visibility = ["//visibility:public"],
    hdrs = ["amqp/amqpsender.h"],
    deps = [
        ":amqp_amqpconfig",
    ],
)


cc_library(
    name = "arith_uint256",
    visibility = ["//visibility:public"],
    srcs = ["arith_uint256.cpp"],
    hdrs = ["arith_uint256.h"],
    deps = [
        ":uint256",
        ":utilstrencodings",
        "//src/crypto:common",
    ],
)

cc_library(
    name = "asyncrpcoperation",
    visibility = ["//visibility:public"],
    srcs = ["asyncrpcoperation.cpp"],
    hdrs = ["asyncrpcoperation.h"],
    deps = [
        "//src/univalue",
        "@boost//:uuid",
    ],
)

cc_library(
    name = "asyncrpcqueue",
    visibility = ["//visibility:public"],
    srcs = ["asyncrpcqueue.cpp"],
    hdrs = ["asyncrpcqueue.h"],
    deps = [
        ":asyncrpcoperation",
    ],
)

cc_library(
    name = "bloom",
    visibility = ["//visibility:public"],
    srcs = ["bloom.cpp"],
    hdrs = ["bloom.h"],
    deps = [
        ":hash",
        ":serialize",
        ":streams",
        ":util",
        "//src/primitives:transaction",
        "//src/script",
        "//src/script:standard",
        "@boost//:foreach",
    ],
)

cc_library(
    name = "chainparamsseeds",
    visibility = ["//visibility:public"],
    hdrs = ["chainparamsseeds.h"],
)

cc_library(
    name = "checkqueue",
    visibility = ["//visibility:public"],
    hdrs = ["checkqueue.h"],
    deps = [
        "@boost//:foreach",
        "@boost//:thread",
    ],
)

cc_library(
    name = "clientversion",
    visibility = ["//visibility:public"],
    hdrs = ["clientversion.h"],
    srcs = ["clientversion.cpp"],
    deps = [
        ":tinyformat",
        "//src/config:bitcoin-config",
        "@boost//:preprocessor",
    ],
)

cc_library(
    name = "coincontrol",
    visibility = ["//visibility:public"],
    hdrs = ["coincontrol.h"],
    deps = [
        "//src/primitives:transaction",
    ],
)

cc_library(
    name = "compat",
    visibility = ["//visibility:public"],
    hdrs = ["compat.h"],
    deps = [
        "//src/config:bitcoin-config",
    ],
)

cc_library(
    name = "compressor",
    visibility = ["//visibility:public"],
    hdrs = ["compressor.h"],
    srcs = ["compressor.cpp"],
    deps = [
        ":hash",
        ":serialize",
        "//src/primitives:transaction",
        "//src/script",
        "//src/script:standard",
    ],
)

cc_library(
    name = "core_memusage",
    visibility = ["//visibility:public"],
    hdrs = ["core_memusage.h"],
    deps = [
        ":memusage",
        "//src/primitives:block",
        "//src/primitives:transaction",
    ],
)

# TODO(per-gron): hash and pubkey have a circular dependency. Untangle.
cc_library(
    name = "hash",
    visibility = ["//visibility:public"],
    srcs = [
        "hash.cpp",
        "pubkey.cpp"
    ],
    hdrs = [
        "hash.h",
        "pubkey.h"
    ],
    deps = [
        ":serialize",
        ":uint256",
        ":version",
        "//src/crypto:common",
        "//src/crypto:hmac_sha512",
        "//src/crypto:ripemd160",
        "//src/crypto:sha256",
        "//src/secp256k1",
    ],
)

cc_library(
    name = "httpserver",
    visibility = ["//visibility:public"],
    hdrs = ["httpserver.h"],
    srcs = ["httpserver.cpp"],
    deps = [
        ":compat",
        ":netbase",
        ":rpcprotocol",
        ":ui_interface",
        ":util",
        "@boost//:algorithm",
        "@boost//:foreach",
        "@boost//:function",
        "@boost//:scoped_ptr",
        "@boost//:thread",
        "@libevent//:event",
    ],
)

cc_library(
    name = "key",
    visibility = ["//visibility:public"],
    srcs = ["key.cpp"],
    hdrs = ["key.h"],
    deps = [
        ":arith_uint256",
        ":hash",
        ":serialize",
        ":uint256",
        ":util",
        "//src/crypto:common",
        "//src/crypto:hmac_sha512",
        "//src/secp256k1",
        "//src/support/allocators:secure",
    ],
)

cc_library(
    name = "keystore",
    visibility = ["//visibility:public"],
    srcs = ["keystore.cpp"],
    hdrs = ["keystore.h"],
    deps = [
        ":hash",
        ":key",
        ":util",
        "//src/script",
        "//src/script:standard",
        "//src/zcash:Address",
        "//src/zcash:NoteEncryption",
        "@boost//:foreach",
        "@boost//:signals2",
        "@boost//:variant",
    ],
)

cc_library(
    name = "leveldbwrapper",
    visibility = ["//visibility:public"],
    srcs = ["leveldbwrapper.cpp"],
    hdrs = ["leveldbwrapper.h"],
    deps = [
        ":clientversion",
        ":serialize",
        ":streams",
        ":util",
        ":version",
        "//src/leveldb",
        "//src/leveldb:memenv",
        "@boost//:filesystem",
    ],
)

cc_library(
    name = "limitedmap",
    visibility = ["//visibility:public"],
    hdrs = ["limitedmap.h"],
)

cc_library(
    name = "memusage",
    visibility = ["//visibility:public"],
    hdrs = ["memusage.h"],
    deps = [
        "@boost//:foreach",
        "@boost//:unordered",
    ],
)

cc_library(
    name = "merkleblock",
    visibility = ["//visibility:public"],
    hdrs = ["merkleblock.h"],
    srcs = ["merkleblock.cpp"],
    deps = [
        ":bloom",
        ":hash",
        ":serialize",
        ":uint256",
        ":utilstrencodings",
        "//src/consensus",
        "//src/primitives:block",
    ],
)

cc_library(
    name = "mruset",
    visibility = ["//visibility:public"],
    hdrs = ["mruset.h"],
)

# TODO(per-gron): These are a tangle of circular dependencies. Untangle.
cc_library(
    name = "main",
    visibility = ["//visibility:public"],
    srcs = [
        "alert.cpp",
        "amqp/amqpnotificationinterface.cpp",
        "amqp/amqppublishnotifier.cpp",
        "base58.cpp",
        "chain.cpp",
        "chainparams.cpp",
        "checkpoints.cpp",
        "coins.cpp",
        "core_read.cpp",
        "core_write.cpp",
        "deprecation.cpp",
        "httprpc.cpp",
        "init.cpp",
        "main.cpp",
        "metrics.cpp",
        "miner.cpp",
        "net.cpp",
        "paymentdisclosure.cpp",
        "paymentdisclosuredb.cpp",
        "policy/fees.cpp",
        "pow.cpp",
        "rest.cpp",
        "rpcblockchain.cpp",
        "rpcmining.cpp",
        "rpcmisc.cpp",
        "rpcnet.cpp",
        "rpcrawtransaction.cpp",
        "rpcserver.cpp",
        "sendalert.cpp",
        "torcontrol.cpp",
        "txdb.cpp",
        "txmempool.cpp",
        "utiltest.cpp",
        "wallet/asyncrpcoperation_sendmany.cpp",
        "wallet/asyncrpcoperation_shieldcoinbase.cpp",
        "wallet/rpcdisclosure.cpp",
        "wallet/rpcdump.cpp",
        "wallet/rpcwallet.cpp",
        "wallet/wallet.cpp",
        "wallet/walletdb.cpp",
        "zcbenchmarks.cpp",
        "zmq/zmqnotificationinterface.cpp",
        "zmq/zmqpublishnotifier.cpp",
    ],
    hdrs = [
        "alert.h",
        "amqp/amqpnotificationinterface.h",
        "amqp/amqppublishnotifier.h",
        "base58.h",
        "chain.h",
        "chainparams.h",
        "checkpoints.h",
        "coins.h",
        "core_io.h",
        "deprecation.h",
        "httprpc.cpp",  # TODO(per-gron): Remove direct #includes of this file
        "httprpc.h",
        "init.h",
        "main.h",
        "metrics.h",
        "miner.h",
        "net.h",
        "paymentdisclosure.h",
        "paymentdisclosuredb.h",
        "policy/fees.h",
        "pow.h",
        "rpcserver.h",
        "torcontrol.cpp",  # TODO(per-gron): Remove direct #includes of this file
        "torcontrol.h",
        "txdb.h",
        "txmempool.h",
        "utiltest.h",
        "wallet/asyncrpcoperation_sendmany.h",
        "wallet/asyncrpcoperation_shieldcoinbase.h",
        "wallet/wallet.h",
        "wallet/walletdb.h",
        "zcbenchmarks.h",
        "zmq/zmqnotificationinterface.h",
        "zmq/zmqpublishnotifier.h",
    ],
    deps = [
        ":addrman",
        ":alertkeys",
        ":amount",
        ":amqp_amqpabstractnotifier",
        ":amqp_amqpconfig",
        ":amqp_amqpsender",
        ":arith_uint256",
        ":asyncrpcoperation",
        ":asyncrpcqueue",
        ":bloom",
        ":chainparamsseeds",
        ":checkqueue",
        ":clientversion",
        ":coincontrol",
        ":compat",
        ":compressor",
        ":core_memusage",
        ":hash",
        ":httpserver",
        ":key",
        ":keystore",
        ":leveldbwrapper",
        ":limitedmap",
        ":memusage",
        ":merkleblock",
        ":mruset",
        ":netbase",
        ":protocol",
        ":rpcprotocol",
        ":scheduler",
        ":serialize",
        ":streams",
        ":timedata",
        ":tinyformat",
        ":ui_interface",
        ":uint256",
        ":undo",
        ":util",
        ":utilmoneystr",
        ":utilstrencodings",
        ":utiltime",
        ":validationinterface",
        ":version",
        ":wallet_crypter",
        ":wallet_db",
        ":wallet_wallet_ismine",
        ":zmq_zmqabstractnotifier",
        "//src/compat:sanity",
        "//src/config:bitcoin-config",
        "//src/consensus",
        "//src/consensus:params",
        "//src/consensus:validation",
        "//src/crypto:common",
        "//src/crypto:equihash",
        "//src/crypto:hmac_sha256",
        "//src/leveldb",
        "//src/pow/tromp:equi_miner",
        "//src/primitives:block",
        "//src/primitives:transaction",
        "//src/script",
        "//src/script:interpreter",
        "//src/script:script_error",
        "//src/script:sigcache",
        "//src/script:sign",
        "//src/script:standard",
        "//src/snark:libsnark",
        "//src/support/allocators:zeroafterfree",
        "//src/univalue",
        "//src/zcash:Address",
        "//src/zcash:IncrementalMerkleTree",
        "//src/zcash:JoinSplit",
        "//src/zcash:Note",
        "//src/zcash:NoteEncryption",
        "//src/zcash:Proof",
        "//src/zcash:Zcash",
        "@boost//:algorithm",
        "@boost//:assign",
        "@boost//:bind",
        "@boost//:date_time",
        "@boost//:dynamic_bitset",
        "@boost//:filesystem",
        "@boost//:foreach",
        "@boost//:function",
        "@boost//:interprocess",
        "@boost//:iostreams",
        "@boost//:math",
        "@boost//:optional",
        "@boost//:scoped_ptr",
        "@boost//:shared_ptr",
        "@boost//:signals2",
        "@boost//:static_assert",
        "@boost//:thread",
        "@boost//:tuple",
        "@boost//:unordered",
        "@boost//:variant",
        "@libevent//:event",
        "@libsodium//:sodium",
        "@openssl//:crypto",
    ],
    defines = [
        "ENABLE_MINING",
        "ENABLE_WALLET"
    ],
)

cc_library(
    name = "netbase",
    visibility = ["//visibility:public"],
    srcs = ["netbase.cpp"],
    hdrs = ["netbase.h"],
    deps = [
        ":compat",
        ":hash",
        ":serialize",
        ":uint256",
        ":util",
        ":utilstrencodings",
        "//src/config:bitcoin-config",
        "@boost//:algorithm",
        "@boost//:thread",
    ],
    linkopts = ["-lanl"],
)

cc_library(
    name = "noui",
    visibility = ["//visibility:public"],
    srcs = ["noui.cpp"],
    hdrs = ["noui.h"],
    deps = [
        ":ui_interface",
        ":util",
    ],
)

cc_library(
    name = "protocol",
    visibility = ["//visibility:public"],
    srcs = ["protocol.cpp"],
    hdrs = ["protocol.h"],
    deps = [
        ":netbase",
        ":serialize",
        ":uint256",
        ":util",
        ":utilstrencodings",
        ":version",
    ],
)

cc_library(
    name = "reverselock",
    visibility = ["//visibility:public"],
    hdrs = ["reverselock.h"],
)

cc_library(
    name = "rpcclient",
    visibility = ["//visibility:public"],
    hdrs = ["rpcclient.h"],
    srcs = ["rpcclient.cpp"],
    deps = [
        ":rpcprotocol",
        ":util",
        "//src/univalue",
    ],
)

cc_library(
    name = "rpcprotocol",
    visibility = ["//visibility:public"],
    hdrs = ["rpcprotocol.h"],
    srcs = ["rpcprotocol.cpp"],
    deps = [
        ":tinyformat",
        ":util",
        ":utilstrencodings",
        ":utiltime",
        ":version",
        "//src/univalue",
        "@boost//:filesystem",
    ],
)

cc_library(
    name = "scheduler",
    visibility = ["//visibility:public"],
    srcs = ["scheduler.cpp"],
    hdrs = ["scheduler.h"],
    deps = [
        ":reverselock",
        "@boost//:bind",
        "@boost//:chrono",
        "@boost//:function",
        "@boost//:thread",
    ],
)

cc_library(
    name = "serialize",
    visibility = ["//visibility:public"],
    hdrs = ["serialize.h"],
    deps = [
        "//src/compat:endian",
        "@boost//:array",
        "@boost//:optional",
    ],
)

cc_library(
    name = "streams",
    visibility = ["//visibility:public"],
    hdrs = ["streams.h"],
    deps = [
        ":serialize",
        "//src/support/allocators:zeroafterfree",
    ],
)

cc_library(
    name = "threadsafety",
    visibility = ["//visibility:public"],
    hdrs = ["threadsafety.h"],
)

cc_library(
    name = "timedata",
    visibility = ["//visibility:public"],
    srcs = ["timedata.cpp"],
    hdrs = ["timedata.h"],
    deps = [
        ":netbase",
        ":ui_interface",
        ":util",
        ":utilstrencodings",
        "@boost//:foreach",
    ],
)

cc_library(
    name = "tinyformat",
    visibility = ["//visibility:public"],
    hdrs = ["tinyformat.h"],
)

cc_library(
    name = "uint252",
    visibility = ["//visibility:public"],
    hdrs = ["uint252.h"],
    deps = [
        ":serialize",
        ":uint256",
    ],
)

cc_library(
    name = "ui_interface",
    visibility = ["//visibility:public"],
    hdrs = ["ui_interface.h"],
    deps = [
        "@boost//:signals2",
    ],
)

cc_library(
    name = "uint256",
    visibility = ["//visibility:public"],
    srcs = ["uint256.cpp"],
    hdrs = ["uint256.h"],
    deps = [
        ":utilstrencodings",
    ],
)

cc_library(
    name = "undo",
    visibility = ["//visibility:public"],
    hdrs = ["undo.h"],
    deps = [
        ":compressor",
        ":serialize",
        "//src/primitives:transaction",
    ],
)

# TODO(per-gron): util, random, sync and chainparamsbase have circular dependencies. Untangle.
cc_library(
    name = "util",
    visibility = ["//visibility:public"],
    srcs = [
        "chainparamsbase.cpp",
        "random.cpp",
        "sync.cpp",
        "util.cpp",
    ],
    hdrs = [
        "chainparamsbase.h",
        "random.h",
        "sync.h",
        "util.h",
    ],
    deps = [
        ":compat",
        ":serialize",
        ":threadsafety",
        ":tinyformat",
        ":uint256",
        ":utilstrencodings",
        ":utiltime",
        "//src/config:bitcoin-config",
        "//src/support:cleanse",
        "@boost//:algorithm",
        "@boost//:filesystem",
        "@boost//:foreach",
        "@boost//:program_options",
        "@boost//:signals2",
        "@boost//:thread",
        "@libsodium//:sodium",
        "@openssl//:crypto",
    ],
)

cc_library(
    name = "utilmoneystr",
    visibility = ["//visibility:public"],
    srcs = ["utilmoneystr.cpp"],
    hdrs = ["utilmoneystr.h"],
    deps = [
        ":amount",
        ":tinyformat",
        ":utilstrencodings",
        "//src/primitives:transaction",
    ],
)

cc_library(
    name = "utiltime",
    visibility = ["//visibility:public"],
    srcs = ["utiltime.cpp"],
    hdrs = ["utiltime.h"],
    deps = [
        "//src/config:bitcoin-config",
        "@boost//:date_time",
        "@boost//:thread",
    ],
)

cc_library(
    name = "utilstrencodings",
    visibility = ["//visibility:public"],
    srcs = ["utilstrencodings.cpp"],
    hdrs = ["utilstrencodings.h"],
    deps = [
        ":tinyformat",
    ],
)

cc_library(
    name = "validationinterface",
    visibility = ["//visibility:public"],
    srcs = ["validationinterface.cpp"],
    hdrs = ["validationinterface.h"],
    deps = [
        "//src/zcash:IncrementalMerkleTree",
        "@boost//:signals2",
    ],
)

cc_library(
    name = "version",
    visibility = ["//visibility:public"],
    hdrs = ["version.h"],
)

# TODO(per-gron): This library ought to be declared in a BUILD file in the
# wallet directory but it's not because there are files there that are part
# of the big cyclic tangle, and Bazel does not allow having a BUILD file
# there when those files are used by libraries in this file.
cc_library(
    name = "wallet_crypter",
    visibility = ["//visibility:public"],
    srcs = ["wallet/crypter.cpp"],
    hdrs = ["wallet/crypter.h"],
    deps = [
        ":keystore",
        ":serialize",
        ":streams",
        ":util",
        "//src/script",
        "//src/script:standard",
        "//src/support/allocators:secure",
        "//src/zcash:Address",
        "@boost//:foreach",
        "@openssl//:crypto",
    ],
)

# TODO(per-gron): This library ought to be declared in a BUILD file in the
# wallet directory but it's not because there are files there that are part
# of the big cyclic tangle, and Bazel does not allow having a BUILD file
# there when those files are used by libraries in this file.
cc_library(
    name = "wallet_db",
    visibility = ["//visibility:public"],
    srcs = ["wallet/db.cpp"],
    hdrs = ["wallet/db.h"],
    deps = [
        ":addrman",
        ":clientversion",
        ":hash",
        ":protocol",
        ":serialize",
        ":streams",
        ":util",
        ":utilstrencodings",
        ":version",
        "@bdb//:db_cxx",
        "@boost//:filesystem",
        "@boost//:thread",
        "@boost//:version",
    ],
)

# TODO(per-gron): This library ought to be declared in a BUILD file in the
# wallet directory but it's not because there are files there that are part
# of the big cyclic tangle, and Bazel does not allow having a BUILD file
# there when those files are used by libraries in this file.
cc_library(
    name = "wallet_wallet_ismine",
    visibility = ["//visibility:public"],
    srcs = ["wallet/wallet_ismine.cpp"],
    hdrs = ["wallet/wallet_ismine.h"],
    deps = [
        ":key",
        ":keystore",
        "//src/script",
        "//src/script:standard",
        "@boost//:foreach",
    ],
)

cc_binary(
    name = "zcash-cli",
    visibility = ["//visibility:public"],
    srcs = ["bitcoin-cli.cpp"],
    deps = [
        ":clientversion",
        ":rpcclient",
        ":rpcprotocol",
        ":util",
        ":utilstrencodings",
        "//src/support:events",
        "//src/univalue",
        "@boost//:filesystem",
        "@libevent//:event",
    ],
)

cc_binary(
    name = "zcash-tx",
    visibility = ["//visibility:public"],
    srcs = ["bitcoin-tx.cpp"],
    deps = [
        ":clientversion",
        ":keystore",
        ":main",
        ":util",
        ":utilmoneystr",
        ":utilstrencodings",
        "//src/consensus",
        "//src/primitives:transaction",
        "//src/script",
        "//src/script:sign",
        "//src/univalue",
        "@boost//:algorithm",
        "@boost//:assign",
    ],
)

cc_binary(
    name = "zcashd",
    visibility = ["//visibility:public"],
    srcs = ["bitcoind.cpp"],
    deps = [
        ":clientversion",
        ":httpserver",
        ":main",
        ":noui",
        ":scheduler",
        ":util",
        "@boost//:algorithm",
        "@boost//:filesystem",
        "@boost//:thread",
    ],
)

# TODO(per-gron): This library ought to be declared in a BUILD file in the
# zmq directory but it's not because there are files there that are part
# of the big cyclic tangle, and Bazel does not allow having a BUILD file
# there when those files are used by libraries in this file.
cc_library(
    name = "zmq_zmqabstractnotifier",
    visibility = ["//visibility:public"],
    srcs = ["zmq/zmqabstractnotifier.cpp"],
    hdrs = ["zmq/zmqabstractnotifier.h"],
    deps = [
        ":util",
        ":zmq_zmqconfig",
    ],
)

# TODO(per-gron): This library ought to be declared in a BUILD file in the
# zmq directory but it's not because there are files there that are part
# of the big cyclic tangle, and Bazel does not allow having a BUILD file
# there when those files are used by libraries in this file.
cc_library(
    name = "zmq_zmqconfig",
    visibility = ["//visibility:public"],
    hdrs = ["zmq/zmqconfig.h"],
    deps = [
        "//src/config:bitcoin-config",
        "//src/primitives:block",
        "//src/primitives:transaction",
        "@zeromq//:zmq",
    ],
    defines = ["ENABLE_ZMQ=1"],
)
