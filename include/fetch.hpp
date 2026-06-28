#ifndef FETCH_HPP_
#define FETCH_HPP_
#include <string>
#include <vector>

struct PricePoint {
  std::string time;  // ISO timestamp
  double price;      // SEK/kWh
};

// Fetches content from a URL using libcurl
std::string http_get(const std::string& url);

// Builds Elprisetjustnu URL for a given date + area
std::string build_url(int year, int month, int day, const std::string& area);

// Fetches today's & tomorrow's price arrays
std::vector<PricePoint> fetch_prices(const std::string& area);

#endif