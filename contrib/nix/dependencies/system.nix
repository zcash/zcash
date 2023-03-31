# Packages that we don’t manage in the current build system.
final: prev: {
  python = prev.python3;

  pythonPackages = prev.python3Packages;
}
