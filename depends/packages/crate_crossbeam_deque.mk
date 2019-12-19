package=crate_crossbeam_deque
$(package)_crate_name=crossbeam-deque
$(package)_version=0.7.2
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=c3aa945d63861bfe624b55d153a39684da1e8c0bc8fba932f7ee3a3c16cea3ca
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
