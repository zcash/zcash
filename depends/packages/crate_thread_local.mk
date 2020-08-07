package=crate_thread_local
$(package)_crate_name=thread_local
$(package)_version=1.0.1
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=d40c6d1b69745a6ec6fb1ca717914848da4b44ae29d9b3080cbee91d72a69b14
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
