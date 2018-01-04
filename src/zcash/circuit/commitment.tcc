template<typename FieldT>
class note_commitment_gadget : gadget<FieldT> {
private:
    std::shared_ptr<block_variable<FieldT>> block1;
    std::shared_ptr<block_variable<FieldT>> block2;
    std::shared_ptr<sha256_compression_function_gadget<FieldT>> hasher1;
    std::shared_ptr<digest_variable<FieldT>> intermediate_hash;
    std::shared_ptr<sha256_compression_function_gadget<FieldT>> hasher2;

public:
    note_commitment_gadget(
        protoboard<FieldT> &pb,
        pb_variable<FieldT>& ZERO,
        pb_variable_array<FieldT>& a_pk,
        pb_variable_array<FieldT>& v,
        pb_variable_array<FieldT>& rho,
        pb_variable_array<FieldT>& r,
        std::shared_ptr<digest_variable<FieldT>> result,
	const std::string &annotation_prefix
			   ) : gadget<FieldT>(pb, FMT(annotation_prefix, " note_commitment_gadget")) {
        pb_variable_array<FieldT> leading_byte =
	  from_bits({1, 0, 1, 1, 0, 0, 0, 0}, ZERO);

        pb_variable_array<FieldT> first_of_rho(rho.begin(), rho.begin()+184);
        pb_variable_array<FieldT> last_of_rho(rho.begin()+184, rho.end());

        intermediate_hash.reset(new digest_variable<FieldT>(pb, 256, FMT(this->annotation_prefix, " intermediate_hash")));

        // final padding
        pb_variable_array<FieldT> length_padding =
            from_bits({
                // padding
                1,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,

                // length of message (840 bits)
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,0,0,
                0,0,0,0,0,0,1,1,
                0,1,0,0,1,0,0,0
            }, ZERO);

        block1.reset(new block_variable<FieldT>(pb, {
            leading_byte,
            a_pk,
            v,
            first_of_rho
	      }, FMT(this->annotation_prefix, " com_block1")));

        block2.reset(new block_variable<FieldT>(pb, {
            last_of_rho,
            r,
            length_padding
	      }, FMT(this->annotation_prefix, " com_block2")));

        pb_linear_combination_array<FieldT> IV = SHA256_default_IV(pb);

        hasher1.reset(new sha256_compression_function_gadget<FieldT>(
            pb,
            IV,
            block1->bits,
            *intermediate_hash,
	    FMT(this->annotation_prefix, " com_hasher1")));

        pb_linear_combination_array<FieldT> IV2(intermediate_hash->bits);

        hasher2.reset(new sha256_compression_function_gadget<FieldT>(
            pb,
            IV2,
            block2->bits,
            *result,
	    FMT(this->annotation_prefix, " com_hasher2")));
    }

    void generate_r1cs_constraints() {
        hasher1->generate_r1cs_constraints();
        hasher2->generate_r1cs_constraints();
    }

    void generate_r1cs_witness() {
        hasher1->generate_r1cs_witness();
        hasher2->generate_r1cs_witness();
    }
};
