package=crate_rand_core
$(package)_crate_name=rand_core
$(package)_version=0.5.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=90bde5296fc891b0cef12a6d03ddccc162ce7b2aff54160af9338f8d40df6d19
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
