#include "zcash/circuit/utils.tcc"

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
    boost::array<std::shared_ptr<digest_variable<FieldT>>, NumInputs> zk_input_hmacs;
    boost::array<std::shared_ptr<digest_variable<FieldT>>, NumOutputs> zk_output_commitments;
    pb_variable_array<FieldT> zk_vpub_old;
    pb_variable_array<FieldT> zk_vpub_new;

    // Aux inputs
    pb_variable<FieldT> ZERO;

public:
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
                alloc_uint256(zk_unpacked_inputs, zk_input_hmacs[i]);
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


    }

    void generate_r1cs_constraints() {
        // The true passed here ensures all the inputs
        // are boolean constrained.
        unpacker->generate_r1cs_constraints(true);

        // Constrain `ZERO`
        generate_r1cs_equals_const_constraint<FieldT>(this->pb, ZERO, FieldT::zero(), "ZERO");
    }

    void generate_r1cs_witness(
        const uint256& phi,
        const uint256& rt,
        const uint256& h_sig,
        const boost::array<JSInput, NumInputs>& inputs,
        const boost::array<Note, NumOutputs>& outputs,
        uint64_t vpub_old,
        uint64_t vpub_new
    ) {
        // Witness `zero`
        this->pb.val(ZERO) = FieldT::zero();

        // This happens last, because only by now are all the
        // verifier inputs resolved.
        unpacker->generate_r1cs_witness_from_bits();
    }

    static r1cs_primary_input<FieldT> witness_map(
        const uint256& rt,
        const uint256& h_sig,
        const boost::array<uint256, NumInputs>& hmacs,
        const boost::array<uint256, NumInputs>& nullifiers,
        const boost::array<uint256, NumOutputs>& commitments,
        uint64_t vpub_old,
        uint64_t vpub_new
    ) {
        std::vector<bool> verify_inputs;

        insert_uint256(verify_inputs, uint256()); // TODO: rt
        insert_uint256(verify_inputs, uint256()); // TODO: h_sig
        
        for (size_t i = 0; i < NumInputs; i++) {
            insert_uint256(verify_inputs, uint256()); // TODO: nullifier
            insert_uint256(verify_inputs, uint256()); // TODO: hmac
        }

        for (size_t i = 0; i < NumOutputs; i++) {
            insert_uint256(verify_inputs, uint256()); // TODO: commitment
        }

        insert_uint64(verify_inputs, 0); // TODO: vpub_old
        insert_uint64(verify_inputs, 0); // TODO: vpub_new

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
            acc += 256; // hmac
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