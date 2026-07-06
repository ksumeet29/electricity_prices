#include "fetch.hpp"
#include <curl/curl.h>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include "parse.hpp"

static size_t write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
  auto* s = static_cast<std::string*>(userp);
  s->append((char*)contents, size * nmemb);
  return size * nmemb;
}

std::string http_get(const std::string& url) {
  CURL* c = curl_easy_init();
  if (!c)
    throw std::runtime_error("curl init failed");

  std::string out;
  curl_easy_setopt(c, CURLOPT_URL, url.c_str());
  curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);

  CURLcode rc = curl_easy_perform(c);
  if (rc != CURLE_OK) {
    curl_easy_cleanup(c);
    throw std::runtime_error("curl error: " +
                             std::string(curl_easy_strerror(rc)));
  }

  long code;
  curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
  curl_easy_cleanup(c);

  if (code < 200 || code >= 300)
    throw std::runtime_error("HTTP status " + std::to_string(code));

  return out;
}

std::string build_url(int year, int month, int day, const std::string& area) {
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

// Parse an ISO timestamp ("2025-07-05T16:00:00+02:00") into a time_t.
// Only the first 16 chars ("YYYY-MM-DDTHH:MM") are used, interpreted as
// local time — consistent with the cutoff computed via localtime() below.
static std::time_t parse_slot_time(const std::string& iso) {
  std::tm tm{};
  std::istringstream ss(iso.substr(0, 16));
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M");
  if (ss.fail())
    throw std::runtime_error("Failed to parse slot timestamp: " + iso);
  tm.tm_isdst = -1;  // let mktime determine DST
  return std::mktime(&tm);
}

// Returns a time_t for the start of the current 15-minute slot.
// e.g. at 16:05 → 16:00; at 16:17 → 16:15; at 16:32 → 16:30.
// Slots starting at or after this value are "current or future" and kept.
static std::time_t current_slot_start() {
  std::time_t now = std::time(nullptr);
  std::tm* t = std::localtime(&now);
  t->tm_min = (t->tm_min / 15) * 15;  // floor to nearest 15-min boundary
  t->tm_sec = 0;
  return std::mktime(t);
}

std::vector<PricePoint> fetch_prices(const std::string& area) {
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
    if (dayOffset == 1)
      add_days(yy, mm, dd, 1);

    std::string url = build_url(yy, mm, dd, area);

    try {
      std::string json = http_get(url);
      auto vec = parse_json_manual(json);
      for (const auto& p : vec) {
        if (parse_slot_time(p.time) >= cutoff)
          result.push_back(p);
      }
    } catch (...) {
      if (dayOffset == 0)
        throw;
      // tomorrow not yet published — skip silently
    }
  }

  return result;
}