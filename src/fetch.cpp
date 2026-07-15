#include "fetch.hpp"
#include "parse.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string& value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

std::string shell_escape(const std::string& value) {
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

std::string http_get_impl(const std::string& url) {
  const std::string script =
      "import sys\n"
      "import ssl\n"
      "import urllib.request\n"
      "ctx = ssl._create_unverified_context()\n"
      "req = urllib.request.Request(sys.argv[1], headers={'User-Agent': 'elpris/1.0'})\n"
      "with urllib.request.urlopen(req, timeout=20, context=ctx) as response:\n"
      "    body = response.read().decode('utf-8', 'replace')\n"
      "    sys.stdout.write(str(response.status) + '\\n' + body)\n";

#ifdef _WIN32
  const std::string temp_path = "elpris_http_fetch.py";
#else
  const std::string temp_path = "/tmp/elpris_http_fetch.py";
#endif

  std::ofstream temp_file(temp_path.c_str(), std::ios::out | std::ios::trunc);
  if (!temp_file.is_open()) {
    throw std::runtime_error("failed to create temporary script file");
  }
  temp_file << script;
  temp_file.close();

#ifdef _WIN32
  const std::string command = "python " + shell_escape(temp_path) + " " + shell_escape(url);
  FILE* pipe = _popen(command.c_str(), "r");
#else
  const std::string command = "python3 " + shell_escape(temp_path) + " " + shell_escape(url);
  FILE* pipe = popen(command.c_str(), "r");
#endif

  if (!pipe) {
    throw std::runtime_error("failed to run Python HTTP client");
  }

  std::string output;
  char buffer[4096];
  while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output.append(buffer);
  }

#ifdef _WIN32
  const int status = _pclose(pipe);
#else
  const int status = pclose(pipe);
#endif
  if (status != 0) {
    throw std::runtime_error("HTTP request failed");
  }

  const size_t first_newline = output.find('\n');
  if (first_newline == std::string::npos) {
    throw std::runtime_error("invalid HTTP response");
  }

  const std::string code_text = trim(output.substr(0, first_newline));
  const long code = std::strtol(code_text.c_str(), nullptr, 10);
  if (code < 200 || code >= 300) {
    throw std::runtime_error("HTTP status " + std::to_string(code));
  }

  const std::string body = output.substr(first_newline + 1);
  if (body.empty()) {
    return std::string();
  }
  return body;
}

}  // namespace

std::string http_get(const std::string& url) {
  return http_get_impl(url);
}

std::string build_url(int year, int month, int day, const std::string& area) {
  // Elprisetjustnu API pattern [1](https://billigel.net/blogg/aktuell-elpriskoll-i-goteborg-dagens-elkostnader-marknadsoversikt/)
  std::ostringstream oss;
  oss << "https://www.elprisetjustnu.se/api/v1/prices/" << year << "/"
      << std::setw(2) << std::setfill('0') << month << "-" << std::setw(2)
      << std::setfill('0') << day << "_" << area << ".json";
  return oss.str();
}

// Parse an ISO timestamp ("2025-07-05T16:00:00+02:00") into a UTC time_t.
// The offset in the input is used so the comparison is consistent regardless
// of the machine's local timezone.
static std::time_t parse_slot_time(const std::string& iso) {
  std::tm tm{};
  std::istringstream ss(iso.substr(0, 16));
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M");
  if (ss.fail())
    throw std::runtime_error("Failed to parse slot timestamp: " + iso);

  tm.tm_isdst = 0;
  std::time_t utc_time = std::mktime(&tm);

  if (iso.size() >= 19 && (iso[16] == '+' || iso[16] == '-')) {
    const int offset_hours = (iso[17] - '0') * 10 + (iso[18] - '0');
    const int offset_minutes = (iso[20] - '0') * 10 + (iso[21] - '0');
    const int offset_seconds = offset_hours * 3600 + offset_minutes * 60;
    if (iso[16] == '+') {
      utc_time -= offset_seconds;
    } else {
      utc_time += offset_seconds;
    }
  }

  return utc_time;
}

// Returns a UTC time_t for the start of the current 15-minute slot.
// e.g. at 16:05 → 16:00; at 16:17 → 16:15; at 16:32 → 16:30.
// Slots starting at or after this value are "current or future" and kept.
static std::time_t current_slot_start() {
  std::time_t now = std::time(nullptr);
  std::tm t{};
  std::tm* utc = std::gmtime(&now);
  if (!utc) {
    throw std::runtime_error("Failed to read current UTC time");
  }
  t = *utc;
  t.tm_min = (t.tm_min / 15) * 15;  // floor to nearest 15-min boundary
  t.tm_sec = 0;
  return std::mktime(&t);
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

  // Keep only slots whose start time >= the start of the current 15-min slot.
  // At 16:05 the 16:00 slot is included (wash can start now); 15:45 and
  // earlier are dropped.
  std::time_t cutoff = current_slot_start();

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
      for (const auto& p : vec) {
        if (parse_slot_time(p.time) >= cutoff)
          result.push_back(p);
      }
    } catch (...) {
      if (dayOffset == 0) {
        throw;
      }
      // tomorrow not available yet → skip
    }
  }

  return result;
}