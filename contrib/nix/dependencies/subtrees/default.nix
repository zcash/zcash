## Dependencies managed via subtrees
##
## Use `./find-current-subtrees.sh` to get information about the subtrees that
## exist.
final: prev: {
  crc32c =
    (prev.crc32c.override {
      staticOnly = true;
    })
    .overrideAttrs (old: {
      # version = "subtree";
      # src = ../../../../src/crc32c;
    });

  ctaes =
    final.runCommand "ctaes" {
      version = "subtree";
      src = ../../../../src/crypto/ctaes;
    } ''
      mkdir -p $out/include
      cp $src/ctaes.c $src/ctaes.h $out/include/
    '';

  leveldb =
    (prev.leveldb.override {
      static = true;
    })
    .overrideAttrs (old: {
      version = "subtree";
      src = ../../../../src/leveldb;

      buildInputs = [final.crc32c];

      # We rely on the internal memenv library. It seems like the original
      # upstream _may_ have fixed this issue? See
      # https://github.com/google/leveldb/issues/236
      postInstall =
        old.postInstall
        + ''
          cp $src/helpers/memenv/memenv.h $dev/include
        '';
    });

  secp256k1 = prev.secp256k1.overrideAttrs (old: {
    version = "subtree";
    src = ../../../../src/secp256k1;
  });

  # not actually a subtree, but it is copied to our source tree
  tinyformat = final.stdenv.mkDerivation {
    pname = "tinyformat";
    version = "local";

    src = ../../../../src;

    CXXFLAGS = "-std=c++11";

    installPhase = ''
      runHook preInstall
      mkdir -p $out/include
      cp tinyformat.h $out/include/
      runHook postInstall
    '';
  };

  univalue = final.stdenv.mkDerivation {
    pname = "univalue";
    version = "subtree";
    src = ../../../../src/univalue;

    nativeBuildInputs = [final.autoreconfHook];
  };
}
