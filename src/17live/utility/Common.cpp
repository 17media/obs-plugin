#include "Common.hpp"

#include <obs.h>

namespace seventeenlive {

std::string GetCurrentLanguage() {
  const char *locale = obs_get_locale();
  if (strcmp(locale, "ja-JP") == 0) {
    return "JP";
  } else if (strcmp(locale, "zh-CN") == 0 || strcmp(locale, "zh-TW") == 0) {
    return "TW";
  } else {
    return "US";
  }
}

} // namespace seventeenlive
