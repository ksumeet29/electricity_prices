#ifndef PARSE_HPP_
#define PARSE_HPP_
#include <string>
#include <vector>
#include "fetch.hpp"

// Manual JSON parser (only extracts time_start and SEK_per_kWh)
std::vector<PricePoint> parse_json_manual(const std::string& json);

struct CheapestResult {
  size_t index;    // start index of cheapest block
  double average;  // average SEK/kWh
  size_t window;   // number of samples in the window
};

// Find the cheapest contiguous window covering `hours` hours
CheapestResult cheapest_window(const std::vector<PricePoint>& v, double hours);

#endif