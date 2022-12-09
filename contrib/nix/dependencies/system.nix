# Packages that we don’t manage in the current build system.
final: previous: {
  python = previous.python3;

  pythonPackages = previous.python3Packages;
}
