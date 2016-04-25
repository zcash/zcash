template<typename FieldT, size_t NumInputs, size_t NumOutputs>
class joinsplit_gadget : gadget<FieldT> {
public:
    joinsplit_gadget(protoboard<FieldT> &pb) : gadget<FieldT>(pb) {
        pb_variable_array<FieldT> test;
        test.allocate(pb, 1);
        pb.set_input_sizes(1);

        // TODO!
    }

    void generate_r1cs_constraints() {
        // TODO!
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
        // TODO!
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
        // todo

        std::vector<FieldT> input_as_field_elements;
        input_as_field_elements.push_back(FieldT::zero());

        return input_as_field_elements;
    }
};