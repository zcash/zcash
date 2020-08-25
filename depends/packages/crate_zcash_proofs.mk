package=crate_zcash_proofs
$(package)_crate_name=zcash_proofs
$(package)_version=0.3.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=a72603377d95702e4d5ed6146135d55cb38057f9e021c19a3247e109ecdf620d
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
