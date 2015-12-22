package=crypto++
$(package)_version=5.6.2
$(package)_download_path=http://www.cryptopp.com/
$(package)_file_name=cryptopp562.zip
$(package)_sha256_hash=5cbfd2fcb4a6b3aab35902e2e0f3b59d9171fee12b3fc2b363e1801dfec53574
$(package)_dependencies=

# SECURITY BUG: _extract_cmds is responsible for verifying the archive
# hash, but does not do so here:
define $(package)_extract_cmds
  unzip $($(package)_source_dir)/$($(package)_file_name)
endef

define $(package)_build_cmds
  $(MAKE) static CXXFLAGS='-DNDEBUG -g -O2 -fPIC'
endef

define $(package)_stage_cmds
  $(MAKE) install PREFIX=$($(package)_staging_dir)$(host_prefix)
endef
