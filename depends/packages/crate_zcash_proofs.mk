package=crate_zcash_proofs
$(package)_crate_name=zcash_proofs
$(package)_version=0.1.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=6f12228d3bff81779e848bc7e7a68f282c717ef2f67a69e6477f4667fbb06078
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
