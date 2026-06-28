#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "fetch.hpp"
#include "parse.hpp"

int main(int argc, char** argv) {
  std::cout << "Executable run begins" << std::endl;

  try {
    std::string area = "SE3";
    if (argc > 1)
      area = argv[1];

    // --- Washing cycle selection ---
    int cycle = 0;

    std::cout << "Select washing cycle:\n"
              << "  1 = Cotton/Heavy  (3.5 h)\n"
              << "  2 = Synthetic     (2.0 h)\n"
              << "  3 = Quick wash    (1.0 h)\n"
              << "Enter choice (1-3): ";
    std::cin >> cycle;

    if (cycle < 1 || cycle > 3) {
      std::cerr << "Invalid washing cycle. Please choose 1, 2, or 3.\n";
      return 1;
    }

    double hours = 0.0;
    std::string cycleLabel;
    switch (cycle) {
      case 1:
        hours = 3.5;
        cycleLabel = "Cycle 1 – Cotton/Heavy (3.5h)";
        break;
      case 2:
        hours = 2.0;
        cycleLabel = "Cycle 2 – Synthetic    (2.0h)";
        break;
      case 3:
        hours = 1.0;
        cycleLabel = "Cycle 3 – Quick wash   (1.0h)";
        break;
    }

    std::cout << "\nWashing programme : " << cycleLabel << "\n"
              << "Area              : " << area << "\n\n";

    // --- Fetch & parse ---
    std::cout << "Fetching prices..." << std::endl;
    auto prices = fetch_prices(area);

    // Collect unique dates
    std::vector<std::string> dates;
    for (const auto& p : prices) {
      if (p.time.size() >= 10) {
        std::string d = p.time.substr(0, 10);
        if (dates.empty() || dates.back() != d)
          dates.push_back(d);
      }
    }

    // --- Find cheapest window ---
    std::cout << "Finding cheapest window..." << std::endl;
    auto r = cheapest_window(prices, hours);

    // --- Results ---
    std::cout << "\nFound " << prices.size() << " data points on ";
    for (size_t i = 0; i < dates.size(); ++i) {
      if (i)
        std::cout << " and ";
      std::cout << dates[i];
    }
    std::cout << "\n\n";

    std::cout << "=== Cheapest slot for " << cycleLabel << " ===\n";
    std::cout << "Avg price  : " << std::fixed << std::setprecision(4)
              << r.average << " SEK/kWh\n";
    std::cout << "Starts at  : " << prices[r.index].time << "\n";
    std::cout << "Total cost : " << std::fixed << std::setprecision(4)
              << r.average * hours << " SEK/KWh\n\n";

    std::cout << "Included intervals:\n";
    for (size_t i = 0; i < r.window; ++i) {
      std::cout << "  " << prices[r.index + i].time << "  " << std::fixed
                << std::setprecision(4) << prices[r.index + i].price
                << " SEK/kWh\n";
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
}