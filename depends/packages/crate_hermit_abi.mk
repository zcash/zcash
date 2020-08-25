package=crate_hermit_abi
$(package)_crate_name=hermit-abi
$(package)_version=0.1.15
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=3deed196b6e7f9e44a2ae8d94225d80302d81208b1bb673fd21fe634645c85a9
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
