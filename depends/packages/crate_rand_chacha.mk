package=crate_rand_chacha
$(package)_crate_name=rand_chacha
$(package)_version=0.2.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=03a2a90da8c7523f554344f921aa97283eadf6ac484a6d2a7d0212fa7f8d6853
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
