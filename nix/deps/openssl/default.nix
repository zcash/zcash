let
  inherit (import ./../../util) nixpkgs requirePlatform;
  inherit (nixpkgs) stdenv;
  sources = import ./../../sources;
in
  stdenv.mkDerivation rec {
    pname = "openssl";
    version = "1.1.1a";
    src = "${sources}/${pname}-${version}.tar.gz";

    configureOptions = [
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
    ] ++ (requirePlatform "[^-]*-[^-]*-linux-gnu" [
      "-fPIC"
      "-Wa,--noexecstack"
    ]) ++ (requirePlatform "x86_64-[^-]*-linux-gnu" [
      "linux-x86_64"
    ]);

    makeTargets = [
      "build_libs"
      "libcrypto.pc"
      "libssl.pc"
      "openssl.pc"
    ];

    builder = ./builder.sh;
    nativeBuildInputs = [
      nixpkgs.perl
    ];
  }

