template<typename FieldT>
class note_gadget : public gadget<FieldT> {
public:
    pb_variable_array<FieldT> value;
    std::shared_ptr<digest_variable<FieldT>> r;

    note_gadget(protoboard<FieldT> &pb) : gadget<FieldT>(pb) {
        value.allocate(pb, 64);
        r.reset(new digest_variable<FieldT>(pb, 256, ""));
    }

    void generate_r1cs_constraints() {
        for (size_t i = 0; i < 64; i++) {
            generate_boolean_r1cs_constraint<FieldT>(
                this->pb,
                value[i],
                "boolean_value"
            );
        }

        r->generate_r1cs_constraints();
    }

    void generate_r1cs_witness(const Note& note) {
        r->bits.fill_with_bits(this->pb, uint256_to_bool_vector(note.r));
        value.fill_with_bits(this->pb, uint64_to_bool_vector(note.value));
    }
};

template<typename FieldT>
class input_note_gadget : public note_gadget<FieldT> {
public:
    std::shared_ptr<digest_variable<FieldT>> a_sk;
    std::shared_ptr<digest_variable<FieldT>> a_pk;
    std::shared_ptr<digest_variable<FieldT>> rho;

    std::shared_ptr<PRF_addr_a_pk_gadget<FieldT>> spend_authority;
    std::shared_ptr<PRF_nf_gadget<FieldT>> expose_nullifiers;

    input_note_gadget(
        protoboard<FieldT>& pb,
        pb_variable<FieldT>& ZERO,
        std::shared_ptr<digest_variable<FieldT>> nullifier
    ) : note_gadget<FieldT>(pb) {
        a_sk.reset(new digest_variable<FieldT>(pb, 252, ""));
        a_pk.reset(new digest_variable<FieldT>(pb, 256, ""));
        rho.reset(new digest_variable<FieldT>(pb, 256, ""));

        spend_authority.reset(new PRF_addr_a_pk_gadget<FieldT>(
            pb,
            ZERO,
            a_sk->bits,
            a_pk
        ));

        expose_nullifiers.reset(new PRF_nf_gadget<FieldT>(
            pb,
            ZERO,
            a_sk->bits,
            rho->bits,
            nullifier
        ));
    }

    void generate_r1cs_constraints() {
        note_gadget<FieldT>::generate_r1cs_constraints();

        a_sk->generate_r1cs_constraints();
        rho->generate_r1cs_constraints();

        // TODO: This constraint may not be necessary if SHA256
        // already boolean constrains its outputs.
        a_pk->generate_r1cs_constraints();

        spend_authority->generate_r1cs_constraints();
        expose_nullifiers->generate_r1cs_constraints();
    }

    void generate_r1cs_witness(const SpendingKey& key, const Note& note) {
        note_gadget<FieldT>::generate_r1cs_witness(note);

        // Witness a_sk for the input
        a_sk->bits.fill_with_bits(
            this->pb,
            trailing252(uint256_to_bool_vector(key))
        );

        // Witness a_pk for a_sk with PRF_addr
        spend_authority->generate_r1cs_witness();

        // [SANITY CHECK] Witness a_pk with note information
        a_pk->bits.fill_with_bits(
            this->pb,
            uint256_to_bool_vector(note.a_pk)
        );

        // Witness rho for the input note
        rho->bits.fill_with_bits(
            this->pb,
            uint256_to_bool_vector(note.rho)
        );

        // Witness the nullifier for the input note
        expose_nullifiers->generate_r1cs_witness();
    }
};
