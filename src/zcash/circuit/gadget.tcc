#include "zcash/circuit/utils.tcc"
#include "zcash/circuit/prfs.tcc"
#include "zcash/circuit/commitment.tcc"
#include "zcash/circuit/merkle.tcc"
#include "zcash/circuit/note.tcc"

template<typename FieldT, size_t NumInputs, size_t NumOutputs>
class joinsplit_gadget : gadget<FieldT> {
private:
    // Verifier inputs
    pb_variable_array<FieldT> zk_packed_inputs;
    pb_variable_array<FieldT> zk_unpacked_inputs;
    std::shared_ptr<multipacking_gadget<FieldT>> unpacker;

    std::shared_ptr<digest_variable<FieldT>> zk_merkle_root;
    std::shared_ptr<digest_variable<FieldT>> zk_h_sig;
    boost::array<std::shared_ptr<digest_variable<FieldT>>, NumInputs> zk_input_nullifiers;
    boost::array<std::shared_ptr<digest_variable<FieldT>>, NumInputs> zk_input_macs;
    boost::array<std::shared_ptr<digest_variable<FieldT>>, NumOutputs> zk_output_commitments;
    pb_variable_array<FieldT> zk_vpub_old;
    pb_variable_array<FieldT> zk_vpub_new;

    // Aux inputs
    pb_variable<FieldT> ZERO;
    std::shared_ptr<digest_variable<FieldT>> zk_phi;
    pb_variable_array<FieldT> zk_total_uint64;

    // Input note gadgets
    boost::array<std::shared_ptr<input_note_gadget<FieldT>>, NumInputs> zk_input_notes;
    boost::array<std::shared_ptr<PRF_pk_gadget<FieldT>>, NumInputs> zk_mac_authentication;

    // Output note gadgets
    boost::array<std::shared_ptr<output_note_gadget<FieldT>>, NumOutputs> zk_output_notes;

public:
    // PRF_pk only has a 1-bit domain separation "nonce"
    // for different macs.
    BOOST_STATIC_ASSERT(NumInputs <= 2);

    // PRF_rho only has a 1-bit domain separation "nonce"
    // for different output `rho`.
    BOOST_STATIC_ASSERT(NumOutputs <= 2);

    joinsplit_gadget(protoboard<FieldT> &pb) : gadget<FieldT>(pb) {
        // Verification
        {
            // The verification inputs are all bit-strings of various
            // lengths (256-bit digests and 64-bit integers) and so we
            // pack them into as few field elements as possible. (The
            // more verification inputs you have, the more expensive
            // verification is.)
            zk_packed_inputs.allocate(pb, verifying_field_element_size());
            pb.set_input_sizes(verifying_field_element_size());

            alloc_uint256(zk_unpacked_inputs, zk_merkle_root);
            alloc_uint256(zk_unpacked_inputs, zk_h_sig);

            for (size_t i = 0; i < NumInputs; i++) {
                alloc_uint256(zk_unpacked_inputs, zk_input_nullifiers[i]);
                alloc_uint256(zk_unpacked_inputs, zk_input_macs[i]);
            }

            for (size_t i = 0; i < NumOutputs; i++) {
                alloc_uint256(zk_unpacked_inputs, zk_output_commitments[i]);
            }

            alloc_uint64(zk_unpacked_inputs, zk_vpub_old);
            alloc_uint64(zk_unpacked_inputs, zk_vpub_new);

            assert(zk_unpacked_inputs.size() == verifying_input_bit_size());

            // This gadget will ensure that all of the inputs we provide are
            // boolean constrained.
            unpacker.reset(new multipacking_gadget<FieldT>(
                pb,
                zk_unpacked_inputs,
                zk_packed_inputs,
                FieldT::capacity(),
                "unpacker"
            ));
        }

        // We need a constant "zero" variable in some contexts. In theory
        // it should never be necessary, but libsnark does not synthesize
        // optimal circuits.
        // 
        // The first variable of our constraint system is constrained
        // to be one automatically for us, and is known as `ONE`.
        ZERO.allocate(pb);

        zk_phi.reset(new digest_variable<FieldT>(pb, 252, ""));

        zk_total_uint64.allocate(pb, 64);

        for (size_t i = 0; i < NumInputs; i++) {
            // Input note gadget for commitments, macs, nullifiers,
            // and spend authority.
            zk_input_notes[i].reset(new input_note_gadget<FieldT>(
                pb,
                ZERO,
                zk_input_nullifiers[i],
                *zk_merkle_root
            ));

            // The input keys authenticate h_sig to prevent
            // malleability.
            zk_mac_authentication[i].reset(new PRF_pk_gadget<FieldT>(
                pb,
                ZERO,
                zk_input_notes[i]->a_sk->bits,
                zk_h_sig->bits,
                i ? true : false,
                zk_input_macs[i]
            ));
        }

        for (size_t i = 0; i < NumOutputs; i++) {
            zk_output_notes[i].reset(new output_note_gadget<FieldT>(
                pb,
                ZERO,
                zk_phi->bits,
                zk_h_sig->bits,
                i ? true : false,
                zk_output_commitments[i]
            ));
        }
    }

    void generate_r1cs_constraints() {
        // The true passed here ensures all the inputs
        // are boolean constrained.
        unpacker->generate_r1cs_constraints(true);

        // Constrain `ZERO`
        generate_r1cs_equals_const_constraint<FieldT>(this->pb, ZERO, FieldT::zero(), "ZERO");

        // Constrain bitness of phi
        zk_phi->generate_r1cs_constraints();

        for (size_t i = 0; i < NumInputs; i++) {
            // Constrain the JoinSplit input constraints.
            zk_input_notes[i]->generate_r1cs_constraints();

            // Authenticate h_sig with a_sk
            zk_mac_authentication[i]->generate_r1cs_constraints();
        }

        for (size_t i = 0; i < NumOutputs; i++) {
            // Constrain the JoinSplit output constraints.
            zk_output_notes[i]->generate_r1cs_constraints();
        }

        // Value balance
        {
            linear_combination<FieldT> left_side = packed_addition(zk_vpub_old);
            for (size_t i = 0; i < NumInputs; i++) {
                left_side = left_side + packed_addition(zk_input_notes[i]->value);
            }

            linear_combination<FieldT> right_side = packed_addition(zk_vpub_new);
            for (size_t i = 0; i < NumOutputs; i++) {
                right_side = right_side + packed_addition(zk_output_notes[i]->value);
            }

            // Ensure that both sides are equal
            this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(
                1,
                left_side,
                right_side
            ));

            // #854: Ensure that left_side is a 64-bit integer.
            for (size_t i = 0; i < 64; i++) {
                generate_boolean_r1cs_constraint<FieldT>(
                    this->pb,
                    zk_total_uint64[i],
                    ""
                );
            }

            this->pb.add_r1cs_constraint(r1cs_constraint<FieldT>(
                1,
                left_side,
                packed_addition(zk_total_uint64)
            ));
        }
    }

    void generate_r1cs_witness(
        const uint252& phi,
        const uint256& rt,
        const uint256& h_sig,
        const boost::array<JSInput, NumInputs>& inputs,
        const boost::array<Note, NumOutputs>& outputs,
        uint64_t vpub_old,
        uint64_t vpub_new
    ) {
        // Witness `zero`
        this->pb.val(ZERO) = FieldT::zero();

        // Witness rt. This is not a sanity check.
        //
        // This ensures the read gadget constrains
        // the intended root in the event that
        // both inputs are zero-valued.
        zk_merkle_root->bits.fill_with_bits(
            this->pb,
            uint256_to_bool_vector(rt)
        );

        // Witness public balance values
        zk_vpub_old.fill_with_bits(
            this->pb,
            uint64_to_bool_vector(vpub_old)
        );
        zk_vpub_new.fill_with_bits(
            this->pb,
            uint64_to_bool_vector(vpub_new)
        );

        {
            // Witness total_uint64 bits
            uint64_t left_side_acc = vpub_old;
            for (size_t i = 0; i < NumInputs; i++) {
                left_side_acc += inputs[i].note.value;
            }

            zk_total_uint64.fill_with_bits(
                this->pb,
                uint64_to_bool_vector(left_side_acc)
            );
        }

        // Witness phi
        zk_phi->bits.fill_with_bits(
            this->pb,
            uint252_to_bool_vector(phi)
        );

        // Witness h_sig
        zk_h_sig->bits.fill_with_bits(
            this->pb,
            uint256_to_bool_vector(h_sig)
        );

        for (size_t i = 0; i < NumInputs; i++) {
            // Witness the input information.
            auto merkle_path = inputs[i].witness.path();
            zk_input_notes[i]->generate_r1cs_witness(
                merkle_path,
                inputs[i].key,
                inputs[i].note
            );

            // Witness macs
            zk_mac_authentication[i]->generate_r1cs_witness();
        }

        for (size_t i = 0; i < NumOutputs; i++) {
            // Witness the output information.
            zk_output_notes[i]->generate_r1cs_witness(outputs[i]);
        }

        // [SANITY CHECK] Ensure that the intended root
        // was witnessed by the inputs, even if the read
        // gadget overwrote it. This allows the prover to
        // fail instead of the verifier, in the event that
        // the roots of the inputs do not match the
        // treestate provided to the proving API.
        zk_merkle_root->bits.fill_with_bits(
            this->pb,
            uint256_to_bool_vector(rt)
        );

        // This happens last, because only by now are all the
        // verifier inputs resolved.
        unpacker->generate_r1cs_witness_from_bits();
    }

    static r1cs_primary_input<FieldT> witness_map(
        const uint256& rt,
        const uint256& h_sig,
        const boost::array<uint256, NumInputs>& macs,
        const boost::array<uint256, NumInputs>& nullifiers,
        const boost::array<uint256, NumOutputs>& commitments,
        uint64_t vpub_old,
        uint64_t vpub_new
    ) {
        std::vector<bool> verify_inputs;

        insert_uint256(verify_inputs, rt);
        insert_uint256(verify_inputs, h_sig);
        
        for (size_t i = 0; i < NumInputs; i++) {
            insert_uint256(verify_inputs, nullifiers[i]);
            insert_uint256(verify_inputs, macs[i]);
        }

        for (size_t i = 0; i < NumOutputs; i++) {
            insert_uint256(verify_inputs, commitments[i]);
        }

        insert_uint64(verify_inputs, vpub_old);
        insert_uint64(verify_inputs, vpub_new);

        assert(verify_inputs.size() == verifying_input_bit_size());
        auto verify_field_elements = pack_bit_vector_into_field_element_vector<FieldT>(verify_inputs);
        assert(verify_field_elements.size() == verifying_field_element_size());
        return verify_field_elements;
    }

    static size_t verifying_input_bit_size() {
        size_t acc = 0;

        acc += 256; // the merkle root (anchor)
        acc += 256; // h_sig
        for (size_t i = 0; i < NumInputs; i++) {
            acc += 256; // nullifier
            acc += 256; // mac
        }
        for (size_t i = 0; i < NumOutputs; i++) {
            acc += 256; // new commitment
        }
        acc += 64; // vpub_old
        acc += 64; // vpub_new

        return acc;
    }

    static size_t verifying_field_element_size() {
        return div_ceil(verifying_input_bit_size(), FieldT::capacity());
    }

    void alloc_uint256(
        pb_variable_array<FieldT>& packed_into,
        std::shared_ptr<digest_variable<FieldT>>& var
    ) {
        var.reset(new digest_variable<FieldT>(this->pb, 256, ""));
        packed_into.insert(packed_into.end(), var->bits.begin(), var->bits.end());
    }

    void alloc_uint64(
        pb_variable_array<FieldT>& packed_into,
        pb_variable_array<FieldT>& integer
    ) {
        integer.allocate(this->pb, 64, "");
        packed_into.insert(packed_into.end(), integer.begin(), integer.end());
    }
};
