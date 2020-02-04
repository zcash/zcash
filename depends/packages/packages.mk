rust_crates := \
  crate_aes \
  crate_aesni \
  crate_aes_soft \
  crate_arrayref \
  crate_arrayvec \
  crate_autocfg \
  crate_bech32 \
  crate_bellman \
  crate_bit_vec \
  crate_blake2b_simd \
  crate_blake2s_simd \
  crate_block_buffer \
  crate_block_cipher_trait \
  crate_block_padding \
  crate_byte_tools \
  crate_byteorder \
  crate_c2_chacha \
  crate_cfg_if \
  crate_constant_time_eq \
  crate_crossbeam_channel \
  crate_crossbeam_deque \
  crate_crossbeam_epoch \
  crate_crossbeam_queue \
  crate_crossbeam_utils \
  crate_crossbeam \
  crate_crypto_api_chachapoly \
  crate_crypto_api \
  crate_digest \
  crate_directories \
  crate_fake_simd \
  crate_ff_derive \
  crate_ff \
  crate_fpe \
  crate_futures_cpupool \
  crate_futures \
  crate_generic_array \
  crate_getrandom \
  crate_group \
  crate_hex \
  crate_lazy_static \
  crate_libc \
  crate_log \
  crate_memoffset \
  crate_nodrop \
  crate_num_bigint \
  crate_num_cpus \
  crate_num_integer \
  crate_num_traits \
  crate_opaque_debug \
  crate_pairing \
  crate_ppv_lite86 \
  crate_proc_macro2 \
  crate_quote \
  crate_rand_chacha \
  crate_rand_core \
  crate_rand_hc \
  crate_rand_os \
  crate_rand_xorshift \
  crate_rand \
  crate_rustc_version \
  crate_scopeguard \
  crate_semver_parser \
  crate_semver \
  crate_sha2 \
  crate_syn \
  crate_typenum \
  crate_unicode_xid \
  crate_wasi \
  crate_winapi_i686_pc_windows_gnu \
  crate_winapi \
  crate_winapi_x86_64_pc_windows_gnu \
  crate_zcash_primitives \
  crate_zcash_proofs
rust_packages := rust $(rust_crates)
proton_packages := proton
zcash_packages := libsodium utfcpp
packages := boost openssl libevent zeromq $(zcash_packages) googletest
native_packages := native_ccache

wallet_packages=bdb

ifneq ($(build_os),darwin)
darwin_native_packages=native_cctools
endif
