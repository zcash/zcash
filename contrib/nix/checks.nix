{
  bash,
  cargo,
  pythonPackages,
  runCommand,
  rustfmt,
  src,
}: {
  cargo-patches =
    runCommand "cargo-patches" {
      inherit src;
    } ''
      ${bash}/bin/bash $src/test/lint/lint-cargo-patches.sh
      mkdir $out
    '';

  include-guards =
    runCommand "include-guards" {
      inherit src;
    } ''
      ${bash}/bin/bash $src/test/lint/lint-include-guards.sh
      mkdir $out
    '';

  pyflakes =
    runCommand "pyflakes" {
      inherit src;
    } ''
      ${pythonPackages.pyflakes}/bin/pyflakes $src/qa $src/src $src/zcutil
      mkdir $out
    '';

  python-utf8-encoding =
    runCommand "python-utf8-encoding" {
      inherit src;
    } ''
      ${bash}/bin/bash $src/test/lint/lint-python-utf8-encoding.sh
      mkdir $out
    '';

  rustfmt =
    runCommand "rustfmt" {
      inherit src;

      nativeBuildInputs = [
        cargo
        rustfmt
      ];
    } ''
      cd $src
      cargo fmt -- --check
      mkdir $out
    '';
}
