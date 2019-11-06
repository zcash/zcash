package=crate_nodrop
$(package)_crate_name=nodrop
$(package)_version=0.1.13
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=2f9667ddcc6cc8a43afc9b7917599d7216aa09c463919ea32c59ed6cac8bc945
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
