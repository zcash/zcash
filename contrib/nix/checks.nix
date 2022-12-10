{
  bash,
  cargo,
  pythonPackages,
  runCommand,
  rustfmt,
  shellcheck,
  src,
}: {
  cargo-patches =
    runCommand "cargo-patches" {
      inherit src;
    } ''
      $src/test/lint/lint-cargo-patches.sh
      mkdir $out
    '';

  include-guards =
    runCommand "include-guards" {
      inherit src;
    } ''
      $src/test/lint/lint-include-guards.sh
      mkdir $out
    '';

  pyflakes =
    runCommand "pyflakes" {
      inherit src;

      nativeBuildInputs = [pythonPackages.pyflakes];
    } ''
      pyflakes $src/qa $src/src $src/zcutil
      mkdir $out
    '';

  python-utf8-encoding =
    runCommand "python-utf8-encoding" {
      inherit src;
    } ''
      $src/test/lint/lint-python-utf8-encoding.sh
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

  shell =
    runCommand "shell" {
      inherit src;

      nativeBuildInputs = [
        shellcheck
      ];
    } ''
      $src/test/lint/lint-shell.sh
      mkdir $out
    '';
}
