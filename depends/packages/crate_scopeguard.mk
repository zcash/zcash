package=crate_scopeguard
$(package)_crate_name=scopeguard
$(package)_version=1.0.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=b42e15e59b18a828bbf5c58ea01debb36b9b096346de35d941dcb89009f24a0d
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
