std::vector<bool> trailing252(std::vector<bool> input) {
    if (input.size() != 256) {
        throw std::length_error("trailing252 input invalid length");
    }

    return std::vector<bool>(input.begin() + 4, input.end());
}

std::vector<bool> uint256_to_bool_vector(uint256 input) {
    std::vector<unsigned char> input_v(input.begin(), input.end());
    std::vector<bool> output_bv(256, 0);
    libzerocash::convertBytesVectorToVector(
        input_v,
        output_bv
    );

    return output_bv;
}

std::vector<bool> uint64_to_bool_vector(uint64_t input) {
    std::vector<unsigned char> num_bv(8, 0);
    libzerocash::convertIntToBytesVector(input, num_bv);
    std::vector<bool> num_v(64, 0);
    libzerocash::convertBytesVectorToVector(num_bv, num_v);

    return num_v;
}

void insert_uint256(std::vector<bool>& into, uint256 from) {
    std::vector<bool> blob = uint256_to_bool_vector(from);
    into.insert(into.end(), blob.begin(), blob.end());
}

void insert_uint64(std::vector<bool>& into, uint64_t from) {
    std::vector<bool> num = uint64_to_bool_vector(from);
    into.insert(into.end(), num.begin(), num.end());
}