package=utfcpp
$(package)_version=3.1.2
$(package)_download_path=https://github.com/nemtrif/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=v$($(package)_version).tar.gz
$(package)_sha256_hash=fea3bfa39fb8bd7368077ea5e1e0db9a8951f7e6fb6d9400b00ab3d92b807c6d

define $(package)_stage_cmds
  cp -a ./source $($(package)_staging_dir)$(host_prefix)/include
endef
