#include "cmake_genex.hpp"

#include <cctype>

namespace gz_cm {

void apply_genex_best_effort(std::string *s) {
  if (s->find("$<") == std::string::npos) return;
  // $<BOOL:ON> / $<BOOL:OFF> / $<BOOL:1> / $<BOOL:0>
  for (int pass = 0; pass < 4; ++pass) {
    bool chg = false;
    for (std::size_t i = 0; i + 4 < s->size(); ++i) {
      if ((*s)[i] != '$' || (*s)[i + 1] != '<') continue;
      std::size_t j = i + 2;
      while (j < s->size() && std::isspace(static_cast<unsigned char>((*s)[j]))) j++;
      if (j + 5 < s->size() && s->compare(j, 5, "BOOL:") == 0) {
        std::size_t k = j + 5;
        while (k < s->size() && std::isspace(static_cast<unsigned char>((*s)[k]))) k++;
        std::size_t vstart = k;
        while (k < s->size() && (*s)[k] != '>' && (*s)[k] != '$') k++;
        std::string val = s->substr(vstart, k - vstart);
        for (char &c : val) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (k < s->size() && (*s)[k] == '>') {
          std::string out = (val == "0" || val == "off" || val == "false" || val == "n" || val == "no" || val == "ignore" || val.empty()) ? "0" : "1";
          s->replace(i, k - i + 1, out);
          chg = true;
          i = 0;
          continue;
        }
      }
    }
    if (!chg) break;
  }
}

}  // namespace gz_cm
