package=crate_crunchy
$(package)_crate_name=crunchy
$(package)_version=0.1.6
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=a2f4a431c5c9f662e1200b7c7f02c34e91361150e382089a8f2dec3ba680cbda
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
