#include "fetch.hpp"
#include "parse.hpp"

int main(int argc, char** argv) {
  // --- Parse CLI args ---
  // Positional arg 1 (if not starting with '-') is treated as area, for
  // backward compatibility with existing callers (e.g. run.py).
  // --area <AREA>, --cycle <1|2|3>, --json are also supported so this
  // executable can be driven non-interactively (e.g. from a web backend).
  std::string area = "SE3";
  int cycle = -1;
  bool json_output = false;

  std::vector<std::string> args(argv + 1, argv + argc);
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--json") {
      json_output = true;
    } else if (args[i] == "--cycle" && i + 1 < args.size()) {
      cycle = std::atoi(args[++i].c_str());
    } else if (args[i] == "--area" && i + 1 < args.size()) {
      area = args[++i];
    } else if (!args[i].empty() && args[i][0] != '-') {
      area = args[i];  // positional area
    }
  }

  if (!json_output)
    std::cout << "Executable run begins" << std::endl;

  try {
    // --- Washing cycle selection ---
    if (cycle == -1) {
      std::cout << "Select washing cycle:\n"
                << "  1 = Cotton/Heavy  (3.5 h)\n"
                << "  2 = Synthetic     (2.0 h)\n"
                << "  3 = Quick wash    (1.0 h)\n"
                << "Enter choice (1-3): ";
      std::cin >> cycle;
    }

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

    if (!json_output)
      std::cout << "\nWashing programme : " << cycleLabel << "\n"
                << "Area              : " << area << "\n\n";

    // --- Fetch & parse ---
    if (!json_output)
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
    if (!json_output)
      std::cout << "Finding cheapest window..." << std::endl;
    auto r = cheapest_window(prices, hours);

    if (json_output) {
      // --- Machine-readable output for callers like a web backend ---
      std::cout << "{\n"
                << "  \"area\": \"" << area << "\",\n"
                << "  \"cycle\": " << cycle << ",\n"
                << "  \"cycle_label\": \"" << cycleLabel << "\",\n"
                << "  \"hours\": " << hours << ",\n"
                << "  \"average_price\": " << std::fixed
                << std::setprecision(4) << r.average << ",\n"
                << "  \"start\": \"" << prices[r.index].time << "\",\n"
                << "  \"total_cost\": " << std::fixed << std::setprecision(4)
                << r.average * hours << ",\n"
                << "  \"intervals\": [\n";
      for (size_t i = 0; i < r.window; ++i) {
        std::cout << "    {\"time\": \"" << prices[r.index + i].time
                   << "\", \"price\": " << std::fixed << std::setprecision(4)
                   << prices[r.index + i].price << "}";
        if (i + 1 < r.window)
          std::cout << ",";
        std::cout << "\n";
      }
      std::cout << "  ]\n"
                << "}\n";
      return 0;
    }

    // --- Results (human readable) ---
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