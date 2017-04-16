package=proton
$(package)_version=0.17.0
$(package)_download_path=http://apache.cs.utah.edu/qpid/proton/$($(package)_version)
$(package)_file_name=qpid-proton-$($(package)_version).tar.gz
$(package)_sha256_hash=6ffd26d3d0e495bfdb5d9fefc5349954e6105ea18cc4bb191161d27742c5a01a
$(package)_dependencies=

define $(package)_preprocess_cmds
  sed -i '1i set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")' proton-c/bindings/cpp/CMakeLists.txt && \
  sed -i '1i set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")' examples/cpp/CMakeLists.txt && \
  sed -i '1i set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")' CMakeLists.txt && \
  sed -i.old 's/qpid-proton SHARED/qpid-proton STATIC/' proton-c/CMakeLists.txt && \
  sed -i.old 's/SASL/_DO_NOT_BUILD_SASL_/' proton-c/CMakeLists.txt && \
  sed -i.old 's/qpid-proton-core SHARED/qpid-proton-core STATIC/' proton-c/CMakeLists.txt && \
  sed -i.old 's/find_package(OpenSSL)/#find_package(OpenSSL)/' proton-c/CMakeLists.txt && \
  sed -i.old 's/qpid-proton-cpp SHARED/qpid-proton-cpp STATIC/' proton-c/bindings/cpp/CMakeLists.txt && \
  sed -i.old 's/DEFAULT_GO ON/DEFAULT_GO OFF/' proton-c/bindings/CMakeLists.txt && \
  mkdir build
endef

define $(package)_config_cmds
  cd build; cmake .. -DCMAKE_CXX_STANDARD=11 -DCMAKE_INSTALL_PREFIX=/ -DSYSINSTALL_BINDINGS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_PYTHON=OFF -DBUILD_PHP=OFF -DBUILD_JAVA=OFF -DBUILD_PERL=OFF -DBUILD_RUBY=OFF -DDEFAULT_GO=OFF
endef

define $(package)_build_cmds
  cd build; $(MAKE) VERBOSE=1
endef

define $(package)_stage_cmds
  cd build; $(MAKE) VERBOSE=1 DESTDIR=$($(package)_staging_prefix_dir) install
endef

