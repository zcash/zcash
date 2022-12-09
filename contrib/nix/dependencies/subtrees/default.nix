## Dependencies managed via subtrees
##
## Use `./find-current-subtrees.sh` to get information about the subtrees that
## exist.
##
## When possible, we pull the `src` from upstream repos, ensuring that the
## sha256 matches the one for our local subtree. If we have local modifications
## (i.e., there is no upstream commit that matches our local subtree), then we
## use the subtree as the `src` directly.
final: previous: {
  crc32c =
    (previous.crc32c.override {
      staticOnly = true;
    })
    .overrideAttrs (old: {
      # src = final.fetchFromGitHub {
      #   owner = "google";
      #   repo = "crc32c";
      #   rev = "224988680f7673cd7c769963d4035cb315aa3388";
      #   sha256 = "sha256-871oBNrdJWbp9oc83i5rex4mC5RiK7Re6SU2uwx3OEI=";
      #   fetchSubmodules = true;
      # };
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
    (previous.leveldb.override {
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

  secp256k1 = previous.secp256k1.overrideAttrs (old: {
    version = "subtree";
    src = ../../../../src/secp256k1;
  });

  univalue = final.stdenv.mkDerivation {
    pname = "univalue";
    version = "subtree";
    src = ../../../../src/univalue;

    nativeBuildInputs = [final.autoreconfHook];
  };

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
}
