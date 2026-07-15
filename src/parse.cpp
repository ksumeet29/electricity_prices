#include "parse.hpp"

// ------------------ JSON PARSER ------------------
std::vector<PricePoint> parse_json_manual(const std::string& json) {
  std::vector<PricePoint> out;
  std::cerr << "Parsing Data Manually" << std::endl;
  size_t pos = 0;
  while (true) {
    size_t tpos = json.find("\"time_start\"", pos);
    if (tpos == std::string::npos)
      break;

    size_t colon = json.find(":", tpos);
    size_t q1 = json.find("\"", colon + 1);
    size_t q2 = json.find("\"", q1 + 1);
    if (q1 == std::string::npos || q2 == std::string::npos)
      break;

    std::string time = json.substr(q1 + 1, q2 - q1 - 1);

    size_t ppos = json.find("\"SEK_per_kWh\"", q2);
    if (ppos == std::string::npos)
      break;

    size_t colon2 = json.find(":", ppos);
    size_t end = json.find_first_of(",}", colon2 + 1);
    if (end == std::string::npos)
      end = json.size();

    std::string num = json.substr(colon2 + 1, end - (colon2 + 1));
    num.erase(std::remove_if(num.begin(), num.end(), ::isspace), num.end());
    double price = std::stod(num);

    out.push_back({time, price});
    pos = end;
  }
  std::cerr << "Parsing Successful" << std::endl;
  return out;
}

// ------------------ TIME DELTA HELPER ------------------
static size_t minutes_between(const std::string& t1, const std::string& t2) {
  std::tm a{}, b{};
  strptime(t1.substr(0, 16).c_str(), "%Y-%m-%dT%H:%M", &a);
  strptime(t2.substr(0, 16).c_str(), "%Y-%m-%dT%H:%M", &b);

  time_t ta = mktime(&a);
  time_t tb = mktime(&b);
  return std::llabs((tb - ta) / 60);
}
// ------------------ WINDOW SIZE HELPER ------------------
static size_t window_for_hours(const std::vector<PricePoint>& v, double hours) {
  size_t totalMinutes = static_cast<size_t>(hours * 60.0);

  if (v.size() < 2)
    return (totalMinutes + 59) / 60;  // fallback: ceil in hourly units

  size_t delta = minutes_between(v[0].time, v[1].time);
  return (totalMinutes + delta - 1) / delta;  // ceiling division
}

// ------------------ CHEAPEST WINDOW ------------------
CheapestResult cheapest_window(const std::vector<PricePoint>& v, double hours) {
  CheapestResult result{0, 1e18, 1};

  const size_t n = v.size();
  const size_t w = window_for_hours(v, hours);
  result.window = w;

  if (n < w) {
    result.index = 0;
    result.average = 1e18;
    return result;
  }

  double bestValue = 1e18;
  size_t bestIndex = 0;

  for (size_t i = 0; i + w <= n; ++i) {
    double sum = 0;
    for (size_t k = 0; k < w; ++k)
      sum += v[i + k].price;

    if (sum < bestValue) {
      bestValue = sum;
      bestIndex = i;
    }
  }

  result.index = bestIndex;
  result.average = bestValue / w;
  return result;
}