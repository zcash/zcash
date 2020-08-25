package=crate_blake2s_simd
$(package)_crate_name=blake2s_simd
$(package)_version=0.5.10
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=ab9e07352b829279624ceb7c64adb4f585dacdb81d35cafae81139ccd617cf44
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
