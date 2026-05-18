#include "config.h"
#include "util.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

uint64_t parse_size(const std::string& s) {
    if (s.empty()) return 0;
    char suffix = std::toupper(s.back());
    std::string num_part = s;
    uint64_t multiplier = 1;

    if (suffix == 'K') { multiplier = 1024ULL; num_part.pop_back(); }
    else if (suffix == 'M') { multiplier = 1024ULL * 1024; num_part.pop_back(); }
    else if (suffix == 'G') { multiplier = 1024ULL * 1024 * 1024; num_part.pop_back(); }

    return std::stoull(num_part) * multiplier;
}

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

int config_load(Config& cfg, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        log_err("Cannot open config file: %s", path.c_str());
        return -1;
    }

    std::string section;
    std::string line;
    bool in_array = false;
    std::string array_key;
    std::vector<std::string> array_values;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);

        // skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#') continue;

        // end of multi-line array
        if (in_array) {
            if (trimmed[0] == ']') {
                in_array = false;
                if (section == "sampling" && array_key == "events") {
                    cfg.events = array_values;
                }
                array_values.clear();
                continue;
            }
            // parse array element: "value",
            std::string val = trimmed;
            // remove trailing comma
            if (!val.empty() && val.back() == ',') val.pop_back();
            val = trim(val);
            val = unquote(val);
            if (!val.empty()) array_values.push_back(val);
            continue;
        }

        // section header
        if (trimmed[0] == '[' && trimmed.back() == ']') {
            section = trimmed.substr(1, trimmed.size() - 2);
            continue;
        }

        // key = value
        auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq));
        std::string value = trim(trimmed.substr(eq + 1));

        // check if value starts an array
        if (!value.empty() && value[0] == '[') {
            // inline array or multi-line?
            if (value.back() == ']') {
                // inline array: [val1, val2]
                std::string inner = value.substr(1, value.size() - 2);
                std::istringstream ss(inner);
                std::string item;
                std::vector<std::string> items;
                while (std::getline(ss, item, ',')) {
                    item = trim(item);
                    item = unquote(item);
                    if (!item.empty()) items.push_back(item);
                }
                if (section == "sampling" && key == "events") cfg.events = items;
            } else {
                // multi-line array
                in_array = true;
                array_key = key;
                array_values.clear();
                // check if there's content after [
                std::string after = trim(value.substr(1));
                if (!after.empty()) {
                    if (!after.empty() && after.back() == ',') after.pop_back();
                    after = trim(after);
                    after = unquote(after);
                    if (!after.empty()) array_values.push_back(after);
                }
            }
            continue;
        }

        value = unquote(value);

        // assign to config
        if (section == "tiers") {
            if (key == "hot_node") cfg.hot_node = std::stoi(value);
            else if (key == "cold_node") cfg.cold_node = std::stoi(value);
        } else if (section == "sampling") {
            if (key == "frequency") cfg.frequency = std::stoi(value);
            else if (key == "window_seconds") cfg.window_seconds = std::stoi(value);
        } else if (section == "classification") {
            if (key == "hot_percentile") cfg.hot_percentile = std::stof(value);
        } else if (section == "migration") {
            if (key == "threads") cfg.threads = std::stoi(value);
            else if (key == "max_pages_per_window") cfg.max_pages_per_window = std::stoi(value);
            else if (key == "region_size") {
                cfg.region_size_str = value;
                cfg.region_size_bytes = parse_size(value);
            }
        } else if (section == "daemon") {
            if (key == "pidfile") cfg.pidfile = value;
            else if (key == "verbose") cfg.verbose = (value == "true" || value == "1");
            else if (key == "log_file") cfg.log_file = value;
            else if (key == "perf_bin") cfg.perf_bin = value;
        }
    }

    // validate region_size
    if (cfg.region_size_bytes == 0) {
        cfg.region_size_bytes = parse_size(cfg.region_size_str);
    }

    return 0;
}
