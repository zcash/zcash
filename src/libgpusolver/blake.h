typedef struct  blake2b_state_s 
{
    uint64_t    h[8];
    uint64_t    bytes;
}               blake2b_state_t; 
void zcash_blake2b_init(blake2b_state_t *st, uint8_t hash_len,
	uint32_t n, uint32_t k);
void zcash_blake2b_update(blake2b_state_t *st, const uint8_t *_msg,  
        uint32_t msg_len, uint32_t is_final);
void zcash_blake2b_final(blake2b_state_t *st, uint8_t *out, uint8_t outlen);
