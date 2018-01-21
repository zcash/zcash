libsnark_headers = [
    "libsnark/algebra/curves/alt_bn128/alt_bn128_g1.hpp",
    "libsnark/algebra/curves/alt_bn128/alt_bn128_g2.hpp",
    "libsnark/algebra/curves/alt_bn128/alt_bn128_init.hpp",
    "libsnark/algebra/curves/alt_bn128/alt_bn128_pairing.hpp",
    "libsnark/algebra/curves/alt_bn128/alt_bn128_pp.hpp",
    "libsnark/algebra/curves/curve_utils.hpp",
    "libsnark/algebra/curves/public_params.hpp",
    "libsnark/algebra/evaluation_domain/domains/basic_radix2_domain.hpp",
    "libsnark/algebra/evaluation_domain/domains/basic_radix2_domain_aux.hpp",
    "libsnark/algebra/evaluation_domain/evaluation_domain.hpp",
    "libsnark/algebra/exponentiation/exponentiation.hpp",
    "libsnark/algebra/fields/bigint.hpp",
    "libsnark/algebra/fields/field_utils.hpp",
    "libsnark/algebra/fields/fp.hpp",
    "libsnark/algebra/fields/fp12_2over3over2.hpp",
    "libsnark/algebra/fields/fp2.hpp",
    "libsnark/algebra/fields/fp6_3over2.hpp",
    "libsnark/algebra/knowledge_commitment/knowledge_commitment.hpp",
    "libsnark/algebra/scalar_multiplication/kc_multiexp.hpp",
    "libsnark/algebra/scalar_multiplication/multiexp.hpp",
    "libsnark/algebra/scalar_multiplication/wnaf.hpp",
    "libsnark/common/assert_except.hpp",
    "libsnark/common/data_structures/accumulation_vector.hpp",
    "libsnark/common/data_structures/merkle_tree.hpp",
    "libsnark/common/data_structures/sparse_vector.hpp",
    "libsnark/common/default_types/ec_pp.hpp",
    "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp",
    "libsnark/common/profiling.hpp",
    "libsnark/common/serialization.hpp",
    "libsnark/common/template_utils.hpp",
    "libsnark/common/utils.hpp",
    "libsnark/gadgetlib1/constraint_profiling.hpp",
    "libsnark/gadgetlib1/examples/simple_example.hpp",
    "libsnark/gadgetlib1/gadget.hpp",
    "libsnark/gadgetlib1/gadgets/basic_gadgets.hpp",
    "libsnark/gadgetlib1/gadgets/gadget_from_r1cs.hpp",
    "libsnark/gadgetlib1/gadgets/hashes/digest_selector_gadget.hpp",
    "libsnark/gadgetlib1/gadgets/hashes/hash_io.hpp",
    "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_aux.hpp",
    "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_components.hpp",
    "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_gadget.hpp",
    "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_authentication_path_variable.hpp",
    "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.hpp",
    "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_tree_check_update_gadget.hpp",
    "libsnark/gadgetlib1/pb_variable.hpp",
    "libsnark/gadgetlib1/protoboard.hpp",
    "libsnark/reductions/r1cs_to_qap/r1cs_to_qap.hpp",
    "libsnark/relations/arithmetic_programs/qap/qap.hpp",
    "libsnark/relations/constraint_satisfaction_problems/r1cs/examples/r1cs_examples.hpp",
    "libsnark/relations/constraint_satisfaction_problems/r1cs/r1cs.hpp",
    "libsnark/relations/variable.hpp",
    "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/examples/run_r1cs_ppzksnark.hpp",
    "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp",
    "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark_params.hpp",
]

cc_library(
    name = "libsnark",
    visibility = ["//visibility:public"],
    includes = [".", "libsnark"],
    # NOTE(per-gron): Because of how Bazel handles include_prefix it is
    # necessary to specify the headers both in hdrs and srcs.
    hdrs = libsnark_headers,
    textual_hdrs = [
        "libsnark/algebra/curves/curve_utils.tcc",
        "libsnark/algebra/evaluation_domain/domains/basic_radix2_domain.tcc",
        "libsnark/algebra/evaluation_domain/domains/basic_radix2_domain_aux.tcc",
        "libsnark/algebra/evaluation_domain/evaluation_domain.tcc",
        "libsnark/algebra/exponentiation/exponentiation.tcc",
        "libsnark/algebra/fields/bigint.tcc",
        "libsnark/algebra/fields/field_utils.tcc",
        "libsnark/algebra/fields/fp.tcc",
        "libsnark/algebra/fields/fp12_2over3over2.tcc",
        "libsnark/algebra/fields/fp2.tcc",
        "libsnark/algebra/fields/fp6_3over2.tcc",
        "libsnark/algebra/fields/fp_aux.tcc",
        "libsnark/algebra/knowledge_commitment/knowledge_commitment.tcc",
        "libsnark/algebra/scalar_multiplication/kc_multiexp.tcc",
        "libsnark/algebra/scalar_multiplication/multiexp.tcc",
        "libsnark/algebra/scalar_multiplication/wnaf.tcc",
        "libsnark/common/data_structures/accumulation_vector.tcc",
        "libsnark/common/data_structures/merkle_tree.tcc",
        "libsnark/common/data_structures/sparse_vector.tcc",
        "libsnark/common/serialization.tcc",
        "libsnark/common/utils.tcc",
        "libsnark/gadgetlib1/examples/simple_example.tcc",
        "libsnark/gadgetlib1/gadget.tcc",
        "libsnark/gadgetlib1/gadgets/basic_gadgets.tcc",
        "libsnark/gadgetlib1/gadgets/gadget_from_r1cs.tcc",
        "libsnark/gadgetlib1/gadgets/hashes/digest_selector_gadget.tcc",
        "libsnark/gadgetlib1/gadgets/hashes/hash_io.tcc",
        "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_aux.tcc",
        "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_components.tcc",
        "libsnark/gadgetlib1/gadgets/hashes/sha256/sha256_gadget.tcc",
        "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_authentication_path_variable.tcc",
        "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_tree_check_read_gadget.tcc",
        "libsnark/gadgetlib1/gadgets/merkle_tree/merkle_tree_check_update_gadget.tcc",
        "libsnark/gadgetlib1/pb_variable.tcc",
        "libsnark/gadgetlib1/protoboard.tcc",
        "libsnark/reductions/r1cs_to_qap/r1cs_to_qap.tcc",
        "libsnark/relations/arithmetic_programs/qap/qap.tcc",
        "libsnark/relations/constraint_satisfaction_problems/r1cs/examples/r1cs_examples.tcc",
        "libsnark/relations/constraint_satisfaction_problems/r1cs/r1cs.tcc",
        "libsnark/relations/variable.tcc",
        "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/examples/run_r1cs_ppzksnark.tcc",
        "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.tcc",
    ],
    srcs = libsnark_headers + [
        "libsnark/algebra/curves/alt_bn128/alt_bn128_g1.cpp",
        "libsnark/algebra/curves/alt_bn128/alt_bn128_g2.cpp",
        "libsnark/algebra/curves/alt_bn128/alt_bn128_init.cpp",
        "libsnark/algebra/curves/alt_bn128/alt_bn128_pairing.cpp",
        "libsnark/algebra/curves/alt_bn128/alt_bn128_pp.cpp",
        "libsnark/common/profiling.cpp",
        "libsnark/common/utils.cpp",
        "libsnark/gadgetlib1/constraint_profiling.cpp",
    ],
    copts = [
        "-Wall",
        "-Wextra",
        "-Wno-unused-parameter",
        "-Wno-comment",
        "-Wfatal-errors",
        "-DBN_SUPPORT_SNARK",
        "-DNO_PT_COMPRESSION=1",
        "-iquote",
        "libsnark/snark/libsnark",
    ],
    defines = [
        "USE_ASM",
        "MONTGOMERY_OUTPUT",
        "CURVE_ALT_BN128",
        "BINARY_OUTPUT",
        "MULTICORE",
    ],
    deps = [
        "@libgmp//:gmp",
        "@libsodium//:sodium",
        "@procps//:procps",
    ],
)

cc_library(
    name = "gtests",
    tags = ["buildcleaner-keep"],
    srcs = ["libsnark/gtests.cpp"],
    deps = [
        ":libsnark",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_bilinearity",
    size = "small",
    srcs = ["libsnark/algebra/curves/tests/test_bilinearity.cpp"],
    deps = [
        ":gtests",
        ":libsnark",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_groups",
    size = "small",
    srcs = ["libsnark/algebra/curves/tests/test_groups.cpp"],
    deps = [
        ":gtests",
        ":libsnark",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_bigint",
    srcs = ["libsnark/algebra/fields/tests/test_bigint.cpp"],
    deps = [
        ":gtests",
        ":libsnark",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_fields",
    size = "small",
    srcs = ["libsnark/algebra/fields/tests/test_fields.cpp"],
    deps = [
        ":gtests",
        ":libsnark",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_sha256_gadget",
    size = "small",
    srcs = ["libsnark/gadgetlib1/gadgets/hashes/sha256/tests/test_sha256_gadget.cpp"],
    deps = [
        ":gtests",
        ":libsnark",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_merkle_tree_gadgets",
    size = "small",
    srcs = ["libsnark/gadgetlib1/gadgets/merkle_tree/tests/test_merkle_tree_gadgets.cpp"],
    deps = [
        ":gtests",
        ":libsnark",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_qap",
    srcs = ["libsnark/relations/arithmetic_programs/qap/tests/test_qap.cpp"],
    deps = [
        ":gtests",
        ":libsnark",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "test_r1cs_ppzksnark",
    srcs = ["libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/tests/test_r1cs_ppzksnark.cpp"],
    deps = [
        ":gtests",
        ":libsnark",
        "@com_google_googletest//:gtest",
    ],
)
