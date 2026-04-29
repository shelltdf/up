#include "file_api_ingest.hpp"

#include <cctype>
#include <set>
#include <string>

static std::string read_json_quoted(const std::string &j, std::size_t quote_pos) {
  if (quote_pos >= j.size() || j[quote_pos] != '"') return {};
  std::size_t u = quote_pos + 1;
  std::string name;
  while (u < j.size()) {
    if (j[u] == '"') break;
    if (j[u] == '\\' && u + 1 < j.size()) {
      name += j[u + 1];
      u += 2;
      continue;
    }
    name += j[u];
    u++;
  }
  return name;
}

std::vector<std::string> ingest_target_names_from_codemodel_json(const std::string &j) {
  std::vector<std::string> out;
  std::set<std::string> seen;
  for (std::size_t p = 0; p < j.size(); ++p) {
    if (j.compare(p, 6, "\"name\"") != 0) continue;
    std::size_t t = p + 6;
    while (t < j.size() && std::isspace(static_cast<unsigned char>(j[t]))) t++;
    if (t >= j.size() || j[t] != ':') continue;
    t++;
    while (t < j.size() && std::isspace(static_cast<unsigned char>(j[t]))) t++;
    if (t >= j.size() || j[t] != '"') continue;
    const std::string name = read_json_quoted(j, t);
    if (name.empty()) {
      p = t;
      continue;
    }
    const std::size_t win0 = p > 4000 ? p - 4000 : 0;
    const std::string win = j.substr(win0, p - win0);
    const bool has_type = win.find("\"type\"") != std::string::npos;
    const bool is_libish = win.find("EXECUTABLE") != std::string::npos || win.find("STATIC_LIBRARY") != std::string::npos ||
                           win.find("SHARED_LIBRARY") != std::string::npos || win.find("MODULE_LIBRARY") != std::string::npos ||
                           win.find("OBJECT_LIBRARY") != std::string::npos;
    if (has_type && is_libish && seen.insert(name).second) out.push_back(name);
    p = t;
  }
  return out;
}
