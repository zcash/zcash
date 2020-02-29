#include "subatomic_utils.h"

#include <cstdlib>
#include <cctype>
#include <string.h>
#include <stdarg.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <string>


namespace fs = std::filesystem;

using t_config_key = std::string;
using t_config_value = std::string;
using t_config_registry = std::unordered_map<t_config_key, t_config_value>;
using t_coin_config_registry = std::unordered_map<std::string, t_config_registry>;
static t_config_registry conf; // conf["rpcuser"]
static t_coin_config_registry conf_coins; // conf_coins["SEC"]["rpcuser"], 

char* construct_json(const char* method, size_t count, ...)
{
  std::string json;
  json += "{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", \"method\":\"";
  json += method;
  json += "\", \"params\": [";
  va_list ap;
  va_start(ap, count);
  for (size_t j = 0; j < count; j++) {
    auto str = std::string(va_arg(ap, char *));
    if (str.empty()) continue;
    json += "lol";
    if (j + 1 < count) {
      json += ",";
    }
  }
  va_end(ap);
  json += "] }";
  std::cout << "json: " << json << std::endl;
  //\"getblockchaininfo\", \"params\": []}";
  return strdup(json.c_str());
}
char* concatenate(const char* s1, const char* s2)
{
  return strdup((std::string(s1) + std::string(s2)).c_str());
}

int count_uppers(const std::string& s)
{
    return std::count_if(s.begin(), s.end(),
                         [](unsigned char c){ return std::isupper(c); } // correct
                        );
}

void dump_cfg() {
  for (auto&& [key, value] : conf) {
    std::cout << key << " " << value << std::endl;
  }

  for (auto&& [key, cfg] : conf_coins) {
    std::cout << "--------------------------------------" << std::endl;
    std::cout << "cfg: " << key << std::endl;
    for (auto&& [key, value] : cfg) {
      std::cout << key << " " << value << std::endl;
    }
  }
}

char* get_komodo_config_path()
{
  #ifdef _WIN32
  fs::path conf_root = fs::path(std::getenv("APPDATA")) / ".komodo/komodo.conf";
  #else
  fs::path conf_root = fs::path(std::getenv("HOME")) / ".komodo/komodo.conf";
  #endif
  return strdup(conf_root.c_str());
}

char* get_komodo_config_parent_path()
{
  #ifdef _WIN32
  fs::path conf_root = fs::path(std::getenv("APPDATA")) / ".komodo";
  #else
  fs::path conf_root = fs::path(std::getenv("HOME")) / ".komodo";
  #endif
  return strdup(conf_root.c_str());
}

bool get_conf_content(const std::string& ticker, std::vector<std::string>& vec_out)
{
  char* path = nullptr;
  if (ticker == "KMD") {
    path = get_komodo_config_path();
  } else {
    path = get_komodo_config_parent_path();
    auto tmp = fs::path(path) / ticker / (ticker + ".conf");
    free(path);
    path = strdup(tmp.c_str());
    std::cout << "ticker conf path: " << path << std::endl;
  }
  std::ifstream ifs(path);
  if (!ifs) {
    return false;
  }
  std::string line;
  while (std::getline(ifs, line))
    {
      if(line.size() > 0)
	vec_out.push_back(line);
    }
  free(path);
  return true;
}

bool is_komodo_config_filled()
{
  return not conf.empty();
}

bool fill_config() {
  auto fill_functor = [](auto&& out_map, auto&& out_lines, const std::string& ticker = "") {
    t_config_registry out;
    for (auto&& cur: out_lines) {
      //! Split left / right side
      if (cur.find("=") != std::string::npos) {
	auto key = cur.substr(0, cur.find("="));
	auto value = cur.substr(cur.find("=") + 1);
	out[key] = value;
      }
    }
    using out_t = std::decay_t<decltype(out_map)>;
    if constexpr(std::is_same_v<t_config_registry, out_t>) {
      out_map = out;
    } else {
      out_map[ticker] = out;
    }
  };
  if (is_komodo_config_filled()) {
    return true;
  }
  std::vector<std::string> out;
  if (not get_conf_content("KMD", out)) {
    return false;
  }

  fill_functor(conf, out);
  //! komodo is filled
  out.clear();
  auto parent_p = get_komodo_config_parent_path();
  for (auto&& cur_dir : fs::directory_iterator(parent_p)) {
    auto ticker = cur_dir.path().filename().string();
    if (fs::is_directory(cur_dir.path()) && fs::exists(fs::path(parent_p) / ticker / (ticker + ".conf"))) {
      get_conf_content(ticker, out);
      fill_functor(conf_coins, out, ticker);
      out.clear();
      std::cout << cur_dir.path().filename().c_str() << std::endl;
    }
    }
  free(parent_p);
  return true;
}

const char* get_rpcport(const char* ticker)
{
  return conf_coins[ticker]["rpcport"].c_str();
}

const char* get_rpcuser(const char* ticker) {
  if (strcmp(ticker, "KMD") == 0) {
    return conf["rpcuser"].c_str();
  } else {
    return conf_coins[ticker]["rpcuser"].c_str();
  }
}

const char* get_rpcpassword(const char* ticker)
{
  if (strcmp(ticker, "KMD") == 0) {
    return conf["rpcpassword"].c_str();
  } else {
    return conf_coins[ticker]["rpcpassword"].c_str();
  }
}
