package=libsnark
$(package)_version=0.1
$(package)_download_path=https://supernetorg.bintray.com/misc/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$(package)-$($(package)_git_commit).tar.gz
$(package)_sha256_hash=47478adc2ae88c448dc736d59dfe007de6478e41e88d2d4d2ff4135a17ee6f90
$(package)_git_commit=3854b20c25e8bc567aab2b558dec84d45f4a3e73

$(package)_dependencies=libgmp libsodium

ifeq ($(build_os),darwin)
define $(package)_set_vars
  $(package)_build_env=CC="$($(package)_cc)" CXX="$($(package)_cxx)"
  $(package)_build_env+=CXXFLAGS="$($(package)_cxxflags) -DBINARY_OUTPUT -DSTATICLIB -DNO_PT_COMPRESSION=1 "
endef
define $(package)_build_cmds
	$(MAKE) lib DEPINST=$(host_prefix) CURVE=ALT_BN128 MULTICORE=1 NO_PROCPS=1 NO_GTEST=1 NO_DOCS=1 STATIC=1 NO_SUPERCOP=1 FEATUREFLAGS=-DMONTGOMERY_OUTPUT OPTFLAGS="-O2 -march=x86-64"
endef
else ifeq ($(host_os),mingw32)
define $(package)_build_cmds
	CXX="x86_64-w64-mingw32-g++-posix" CXXFLAGS="-DBINARY_OUTPUT -DPTW32_STATIC_LIB -DSTATICLIB -DNO_PT_COMPRESSION=1 -fopenmp" $(MAKE) lib DEPINST=$(host_prefix) CURVE=ALT_BN128 MULTICORE=1 NO_PROCPS=1 NO_GTEST=1 NO_DOCS=1 STATIC=1 NO_SUPERCOP=1 FEATUREFLAGS=-DMONTGOMERY_OUTPUT OPTFLAGS="-O2 -march=x86-64"
endef
else
define $(package)_build_cmds
	CXXFLAGS="-fPIC -DBINARY_OUTPUT -DNO_PT_COMPRESSION=1" $(MAKE) lib DEPINST=$(host_prefix) CURVE=ALT_BN128 MULTICORE=1 NO_PROCPS=1 NO_GTEST=1 NO_DOCS=1 STATIC=1 NO_SUPERCOP=1 FEATUREFLAGS=-DMONTGOMERY_OUTPUT OPTFLAGS="-O2 -march=x86-64"
endef
endif


define $(package)_stage_cmds
  $(MAKE) install STATIC=1 DEPINST=$(host_prefix) PREFIX=$($(package)_staging_dir)$(host_prefix) CURVE=ALT_BN128 NO_SUPERCOP=1
endef
