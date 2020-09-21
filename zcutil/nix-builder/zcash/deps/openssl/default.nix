with import ./../../../pkgs-pinned.nix;
with import ./../utils.nix;
stdenv.mkDerivation rec {
  pname = "openssl";
  version = "1.1.1a";
  src = fetchurl {
    url = "https://www.openssl.org/source/old/1.1.1/${pname}-${version}.tar.gz";
    sha256 = "fc20130f8b7cbd2fb918b2f14e2f429e109c31ddd0fb38fc5d71d9ffed3f9f41";
  };

  configureOptions = selectByHostPlatform [
    [ # All host platforms:
      "no-afalgeng"
      "no-asm"
      "no-async"
      "no-bf"
      "no-blake2"
      "no-camellia"
      "no-capieng"
      "no-cast"
      "no-chacha"
      "no-cmac"
      "no-cms"
      "no-comp"
      "no-crypto-mdebug"
      "no-crypto-mdebug-backtrace"
      "no-ct"
      "no-des"
      "no-dgram"
      "no-dsa"
      "no-dso"
      "no-dtls"
      "no-dtls1"
      "no-dtls1-method"
      "no-dynamic-engine"
      "no-ec2m"
      "no-ec_nistp_64_gcc_128"
      "no-egd"
      "no-engine"
      "no-err"
      "no-gost"
      "no-heartbeats"
      "no-idea"
      "no-md2"
      "no-md4"
      "no-mdc2"
      "no-multiblock"
      "no-nextprotoneg"
      "no-ocb"
      "no-ocsp"
      "no-poly1305"
      "no-posix-io"
      "no-psk"
      "no-rc2"
      "no-rc4"
      "no-rc5"
      "no-rdrand"
      "no-rfc3779"
      "no-rmd160"
      "no-scrypt"
      "no-sctp"
      "no-seed"
      "no-shared"
      "no-sock"
      "no-srp"
      "no-srtp"
      "no-ssl"
      "no-ssl3"
      "no-ssl3-method"
      "no-ssl-trace"
      "no-stdio"
      "no-tls"
      "no-tls1"
      "no-tls1-method"
      "no-ts"
      "no-ui"
      "no-unit-test"
      "no-weak-ssl-ciphers"
      "no-whirlpool"
      "no-zlib"
      "no-zlib-dynamic"
      # $($(package)_cflags) $($(package)_cppflags)
      "-DPURIFY"
    ]
    {
      kernel = "darwin";
      cpu = "x86_64";
      values = ["darwin64-x86_64-cc"];
    }
    {
      kernel = "freebsd";
      values = [
        "-fPIC"
        "-Wa,--noexecstack"
      ];
    }
    {
      kernel = "freebsd";
      cpu = "i686";
      values = ["BSD-generic32"];
    }
    {
      kernel = "freebsd";
      cpu = "x86_64";
      values = ["BSD-x86_64"];
    }
    {
      kernel = "linux";
      values = [
        "-fPIC"
        "-Wa,--noexecstack"
      ];
    }
    {
      kernel = "linux";
      cpu = "aarch64";
      values = ["linux-generic64"];
    }
    {
      kernel = "linux";
      cpu = "arm";
      values = ["linux-generic32"];
    }
    {
      kernel = "linux";
      cpu = "i686";
      values = ["linux-generic32"];
    }
    {
      kernel = "linux";
      cpu = "mips";
      values= ["linux-generic32"];
    }
    {
      kernel = "linux";
      cpu = "mipsel";
      values = ["linux-generic32"];
    }
    {
      kernel = "linux";
      cpu = "powerpc";
      values = ["linux-generic32"];
    }
    {
      kernel = "linux";
      cpu = "x86_64";
      values = ["linux-x86_64"];
    }
    # FIXME: how to handle mingw32?
    # i686 = "mingw";
    # x86_64 = "mingw64";
  ];

  makeTargets = [
    "build_libs"
    "libcrypto.pc"
    "libssl.pc"
    "openssl.pc"
  ];

  builder = ./builder.sh;
  nativeBuildInputs = [
    perl
  ];
}

