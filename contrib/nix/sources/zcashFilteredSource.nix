let
  inherit (import ../util) config idevName selectSource;
  inherit (config.zcash) buildSource;
  name = idevName "filtered-source";
in
  selectSource name "." buildSource
