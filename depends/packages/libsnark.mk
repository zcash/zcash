package=libsnark
$(package)_download_path=https://github.com/ZencashOfficial/$(package)/releases/download/v20170131/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz

$(package)_sha256_hash=0c243deec42b133948cd5d77848f7988a79e82e5bf570c39d9a96ee7a42f5302
$(package)_git_commit=3854b20c25e8bc567aab2b558dec84d45f4a3e73

define $(package)_set_vars
    $(package)_build_env=CC="$($(package)_cc)" CXX="$($(package)_cxx)"
    $(package)_build_env+=CXXFLAGS="$($(package)_cxxflags) -DBINARY_OUTPUT -DSTATICLIB -DNO_PT_COMPRESSION=1 "
endef

$(package)_dependencies=libgmp libsodium

define $(package)_build_cmds
    $(MAKE) lib DEPINST=$(host_prefix) CURVE=ALT_BN128 MULTICORE=1 NO_PROCPS=1 NO_GTEST=1 NO_DOCS=1 STATIC=1 NO_SUPERCOP=1 FEATUREFLAGS=-DMONTGOMERY_OUTPUT OPTFLAGS="-O2 -march=x86-64"
endef

define $(package)_stage_cmds
    $(MAKE) install STATIC=1 DEPINST=$(host_prefix) PREFIX=$($(package)_staging_dir)$(host_prefix) CURVE=ALT_BN128 NO_SUPERCOP=1
endef
