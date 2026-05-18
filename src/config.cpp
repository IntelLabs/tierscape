#include "config.h"
#include "util.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_set>

uint64_t parse_size(const std::string& s) {
    if (s.empty()) return 0;
    char suffix = static_cast<char>(std::toupper(static_cast<unsigned char>(s.back())));
    std::string num_part = s;
    uint64_t multiplier = 1;
    if (suffix == 'K') { multiplier = 1024ULL;             num_part.pop_back(); }
    else if (suffix == 'M') { multiplier = 1024ULL * 1024; num_part.pop_back(); }
    else if (suffix == 'G') { multiplier = 1024ULL * 1024 * 1024; num_part.pop_back(); }
    try {
        return std::stoull(num_part) * multiplier;
    } catch (...) {
        return 0;
    }
}

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

namespace {
// Known keys per section — used to warn on typos.
const std::unordered_set<std::string> k_known_tiers      = {"hot_node", "cold_node"};
const std::unordered_set<std::string> k_known_sampling   = {"events", "frequency", "window_seconds"};
const std::unordered_set<std::string> k_known_classif    = {"hot_percentile"};
const std::unordered_set<std::string> k_known_migration  = {"threads", "max_pages_per_window", "region_size", "max_idle_windows"};
const std::unordered_set<std::string> k_known_daemon     = {"pidfile", "verbose", "log_file", "perf_bin"};

void warn_unknown(const std::string& section, const std::string& key) {
    log_warn("Unknown config key: [%s].%s (ignored)", section.c_str(), key.c_str());
}
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
        if (trimmed.empty() || trimmed[0] == '#') continue;

        if (in_array) {
            if (trimmed[0] == ']') {
                in_array = false;
                if (section == "sampling" && array_key == "events") {
                    cfg.events = array_values;
                }
                array_values.clear();
                continue;
            }
            std::string val = trimmed;
            if (!val.empty() && val.back() == ',') val.pop_back();
            val = trim(val);
            val = unquote(val);
            if (!val.empty()) array_values.push_back(val);
            continue;
        }

        if (trimmed[0] == '[' && trimmed.back() == ']') {
            section = trimmed.substr(1, trimmed.size() - 2);
            continue;
        }

        auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key   = trim(trimmed.substr(0, eq));
        std::string value = trim(trimmed.substr(eq + 1));

        if (!value.empty() && value[0] == '[') {
            if (value.back() == ']') {
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
                else warn_unknown(section, key);
            } else {
                in_array  = true;
                array_key = key;
                array_values.clear();
                std::string after = trim(value.substr(1));
                if (!after.empty()) {
                    if (after.back() == ',') after.pop_back();
                    after = trim(after);
                    after = unquote(after);
                    if (!after.empty()) array_values.push_back(after);
                }
            }
            continue;
        }

        value = unquote(value);

        try {
            if (section == "tiers") {
                if (!k_known_tiers.count(key)) { warn_unknown(section, key); continue; }
                if (key == "hot_node")  cfg.hot_node  = std::stoi(value);
                else if (key == "cold_node") cfg.cold_node = std::stoi(value);
            } else if (section == "sampling") {
                if (!k_known_sampling.count(key)) { warn_unknown(section, key); continue; }
                if (key == "frequency")           cfg.frequency      = std::stoi(value);
                else if (key == "window_seconds") cfg.window_seconds = std::stoi(value);
            } else if (section == "classification") {
                if (!k_known_classif.count(key)) { warn_unknown(section, key); continue; }
                if (key == "hot_percentile") cfg.hot_percentile = std::stof(value);
            } else if (section == "migration") {
                if (!k_known_migration.count(key)) { warn_unknown(section, key); continue; }
                if (key == "threads") cfg.threads = std::stoi(value);
                else if (key == "max_pages_per_window") cfg.max_pages_per_window = std::stoull(value);
                else if (key == "max_idle_windows")     cfg.max_idle_windows     = std::stoi(value);
                else if (key == "region_size") {
                    cfg.region_size_str   = value;
                    cfg.region_size_bytes = parse_size(value);
                }
            } else if (section == "daemon") {
                if (!k_known_daemon.count(key)) { warn_unknown(section, key); continue; }
                if (key == "pidfile")        cfg.pidfile  = value;
                else if (key == "verbose")   cfg.verbose  = (value == "true" || value == "1");
                else if (key == "log_file")  cfg.log_file = value;
                else if (key == "perf_bin")  cfg.perf_bin = value;
            } else if (!section.empty()) {
                warn_unknown(section, key);
            }
        } catch (const std::exception& e) {
            log_warn("Config: failed to parse [%s].%s = %s (%s)",
                     section.c_str(), key.c_str(), value.c_str(), e.what());
        }
    }

    if (cfg.region_size_bytes == 0) {
        cfg.region_size_bytes = parse_size(cfg.region_size_str);
    }
    return 0;
}
