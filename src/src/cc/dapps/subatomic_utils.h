#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

  char* get_komodo_config_path();
  bool is_komodo_config_filled();
  bool fill_config(); //! Fill configs
  const char* get_cfg_value(const char* ticker, const char* field); //! return NULL if not found
  const char* get_rpcuser(const char* ticker);
  const char* get_rpcpassword(const char* ticker);
  const char* get_rpcport(const char* ticker); //! only for non KMD
  char* concatenate(const char* s1, const char* s2); //! need to be free
  char* construct_json(const char* method, size_t count, ...);
  void dump_cfg();
#ifdef __cplusplus
}
#endif
