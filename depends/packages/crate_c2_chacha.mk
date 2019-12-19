package=crate_c2_chacha
$(package)_crate_name=c2-chacha
$(package)_version=0.2.3
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=214238caa1bf3a496ec3392968969cab8549f96ff30652c9e56885329315f6bb
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
