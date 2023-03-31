## NB: Many of these checks call Bash explicitly rather than relying on the
##     shebang line. We could alternatively use `patchShebangs`, but that
##     currently makes the code noisier. I think `patchShebangs` is better
##     because if the shebang changes, so does the tool used to run it, whereas
##     this way we have to manually update this file as well.
{
  bash,
  cargo,
  git,
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

      nativeBuildInputs = [pythonPackages.pyflakes];
    } ''
      pyflakes $src/qa $src/src $src/zcutil
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

  shell =
    runCommand "shell" {
      inherit src;

      nativeBuildInputs = [
        git
        shellcheck
      ];
    } ''
      ${bash}/bin/bash $src/test/lint/lint-shell.sh
      mkdir $out
    '';
}
