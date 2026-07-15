#include "fetch.hpp"
#include "parse.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

static std::string shell_escape(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size() + 2);
  escaped.push_back('"');
  for (char ch : value) {
    if (ch == '"') {
      escaped += "\\\"";
    } else {
      escaped.push_back(ch);
    }
  }
  escaped.push_back('"');
  return escaped;
}

std::string http_get(const std::string& url) {
#ifdef _WIN32
  const std::string command = "curl.exe -L -s -w \"\\n%{http_code}\" " + shell_escape(url);
  FILE* pipe = _popen(command.c_str(), "r");
#else
  const std::string command = "curl -L -s -w \"\\n%{http_code}\" " + shell_escape(url);
  FILE* pipe = popen(command.c_str(), "r");
#endif
  if (!pipe) {
    throw std::runtime_error("failed to run curl");
  }

  std::string out;
  char buffer[4096];
  while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    out.append(buffer);
  }

#ifdef _WIN32
  const int status = _pclose(pipe);
#else
  const int status = pclose(pipe);
#endif
  if (status != 0) {
    throw std::runtime_error("curl request failed");
  }

  const size_t pos = out.find_last_of('\n');
  if (pos == std::string::npos) {
    throw std::runtime_error("curl response missing status line");
  }

  const std::string body = out.substr(0, pos);
  const std::string status_text = out.substr(pos + 1);
  const long code = std::strtol(status_text.c_str(), nullptr, 10);
  if (code < 200 || code >= 300) {
    throw std::runtime_error("HTTP status " + std::to_string(code));
  }

  return body;
}

std::string build_url(int year, int month, int day, const std::string& area) {
  // Elprisetjustnu API pattern [1](https://billigel.net/blogg/aktuell-elpriskoll-i-goteborg-dagens-elkostnader-marknadsoversikt/)
  std::ostringstream oss;
  oss << "https://www.elprisetjustnu.se/api/v1/prices/" << year << "/"
      << std::setw(2) << std::setfill('0') << month << "-" << std::setw(2)
      << std::setfill('0') << day << "_" << area << ".json";
  return oss.str();
}

static void add_days(int& y, int& m, int& d, int delta) {
  std::tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  std::time_t tt = std::mktime(&t);
  tt += delta * 86400;
  std::tm* nt = std::localtime(&tt);
  y = nt->tm_year + 1900;
  m = nt->tm_mon + 1;
  d = nt->tm_mday;
}

std::vector<PricePoint> fetch_prices(const std::string& area) {
  // today's date
  std::time_t now = std::time(nullptr);
  std::tm* t = std::localtime(&now);
  int y = t->tm_year + 1900;
  int m = t->tm_mon + 1;
  int d = t->tm_mday;

  std::vector<PricePoint> result;

  for (int dayOffset = 0; dayOffset < 2; ++dayOffset) {
    int yy = y, mm = m, dd = d;
    if (dayOffset == 1) {
      add_days(yy, mm, dd, 1);
    }

    std::string url = build_url(yy, mm, dd, area);

    try {
      std::string json = http_get(url);
      auto vec = parse_json_manual(json);
      result.insert(result.end(), vec.begin(), vec.end());
    } catch (...) {
      if (dayOffset == 0) {
        throw;
      }
      // tomorrow not available yet → skip
    }
  }

  return result;
}