package=utfcpp
$(package)_version=3.1
$(package)_download_path=https://github.com/nemtrif/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=ab531c3fd5d275150430bfaca01d7d15e017a188183be932322f2f651506b096

define $(package)_stage_cmds
  cp -a ./source $($(package)_staging_dir)$(host_prefix)/include
endef
