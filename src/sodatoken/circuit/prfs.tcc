template<typename FieldT>
class PRF_gadget : gadget<FieldT> {
private:
    std::shared_ptr<block_variable<FieldT>> block;
    std::shared_ptr<sha256_compression_function_gadget<FieldT>> hasher;
    std::shared_ptr<digest_variable<FieldT>> result;

public:
    PRF_gadget(
        protoboard<FieldT>& pb,
        pb_variable<FieldT>& ZERO,
        bool a,
        bool b,
        bool c,
        bool d,
        pb_variable_array<FieldT> x,
        pb_variable_array<FieldT> y,
        std::shared_ptr<digest_variable<FieldT>> result
    ) : gadget<FieldT>(pb), result(result) {

        pb_linear_combination_array<FieldT> IV = SHA256_default_IV(pb);

        pb_variable_array<FieldT> discriminants;
        discriminants.emplace_back(a ? ONE : ZERO);
        discriminants.emplace_back(b ? ONE : ZERO);
        discriminants.emplace_back(c ? ONE : ZERO);
        discriminants.emplace_back(d ? ONE : ZERO);

        block.reset(new block_variable<FieldT>(pb, {
            discriminants,
            x,
            y
        }, "PRF_block"));

        hasher.reset(new sha256_compression_function_gadget<FieldT>(
            pb,
            IV,
            block->bits,
            *result,
        "PRF_hasher"));
    }

    void generate_r1cs_constraints() {
        hasher->generate_r1cs_constraints();
    }

    void generate_r1cs_witness() {
        hasher->generate_r1cs_witness();
    }
};

template<typename FieldT>
pb_variable_array<FieldT> gen256zeroes(pb_variable<FieldT>& ZERO) {
    pb_variable_array<FieldT> ret;
    while (ret.size() < 256) {
        ret.emplace_back(ZERO);
    }

    return ret;
}

template<typename FieldT>
class PRF_addr_a_pk_gadget : public PRF_gadget<FieldT> {
public:
    PRF_addr_a_pk_gadget(
        protoboard<FieldT>& pb,
        pb_variable<FieldT>& ZERO,
        pb_variable_array<FieldT>& a_sk,
        std::shared_ptr<digest_variable<FieldT>> result
    ) : PRF_gadget<FieldT>(pb, ZERO, 1, 1, 0, 0, a_sk, gen256zeroes(ZERO), result) {}
};

template<typename FieldT>
class PRF_nf_gadget : public PRF_gadget<FieldT> {
public:
    PRF_nf_gadget(
        protoboard<FieldT>& pb,
        pb_variable<FieldT>& ZERO,
        pb_variable_array<FieldT>& a_sk,
        pb_variable_array<FieldT>& rho,
        std::shared_ptr<digest_variable<FieldT>> result
    ) : PRF_gadget<FieldT>(pb, ZERO, 1, 1, 1, 0, a_sk, rho, result) {}
};

template<typename FieldT>
class PRF_pk_gadget : public PRF_gadget<FieldT> {
public:
    PRF_pk_gadget(
        protoboard<FieldT>& pb,
        pb_variable<FieldT>& ZERO,
        pb_variable_array<FieldT>& a_sk,
        pb_variable_array<FieldT>& h_sig,
        bool nonce,
        std::shared_ptr<digest_variable<FieldT>> result
    ) : PRF_gadget<FieldT>(pb, ZERO, 0, nonce, 0, 0, a_sk, h_sig, result) {}
};

template<typename FieldT>
class PRF_rho_gadget : public PRF_gadget<FieldT> {
public:
    PRF_rho_gadget(
        protoboard<FieldT>& pb,
        pb_variable<FieldT>& ZERO,
        pb_variable_array<FieldT>& phi,
        pb_variable_array<FieldT>& h_sig,
        bool nonce,
        std::shared_ptr<digest_variable<FieldT>> result
    ) : PRF_gadget<FieldT>(pb, ZERO, 0, nonce, 1, 0, phi, h_sig, result) {}
};
