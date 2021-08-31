package=utfcpp
$(package)_version=3.2.1
$(package)_download_path=https://github.com/nemtrif/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=8d6aa7d77ad0abb35bb6139cb9a33597ac4c5b33da6a004ae42429b8598c9605

define $(package)_stage_cmds
  cp -a ./source $($(package)_staging_dir)$(host_prefix)/include
endef
