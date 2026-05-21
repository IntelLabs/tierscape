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

    // Strip whitespace.
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    if (b == e) return 0;

    // Optional unit suffix (K/M/G/T, with optional 'B' or 'iB').
    uint64_t mult = 1;
    size_t end = e;
    auto eat = [&](char c) {
        if (end > b && std::toupper(static_cast<unsigned char>(s[end - 1])) == c) {
            --end; return true;
        }
        return false;
    };
    // Allow trailing 'B' or 'iB'.
    eat('B');
    eat('I');
    if (end > b) {
        char u = static_cast<char>(std::toupper(static_cast<unsigned char>(s[end - 1])));
        switch (u) {
            case 'K': mult = 1024ULL;                             --end; break;
            case 'M': mult = 1024ULL * 1024;                      --end; break;
            case 'G': mult = 1024ULL * 1024 * 1024;               --end; break;
            case 'T': mult = 1024ULL * 1024 * 1024 * 1024;        --end; break;
            default:  break;
        }
    }
    if (end <= b) return 0;

    // Digits only.
    for (size_t i = b; i < end; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return 0;
    }

    uint64_t v = 0;
    try {
        v = std::stoull(s.substr(b, end - b));
    } catch (...) {
        return 0;
    }
    // Overflow guard.
    if (mult != 0 && v > (UINT64_MAX / mult)) return 0;
    return v * mult;
}

// Strict allow-list for perf event names: prevents shell injection when
// events are interpolated into perf command lines.
bool is_valid_event_name(const std::string& s) {
    if (s.empty() || s.size() > 255) return false;
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) continue;
        // Common perf event punctuation.
        if (c == '_' || c == '.' || c == ':' || c == '/' ||
            c == '-' || c == '@' || c == '=' || c == ',') continue;
        return false;
    }
    return true;
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

bool key_known(const std::string& section, const std::string& key) {
    if (section == "tiers")          return k_known_tiers.count(key) > 0;
    if (section == "sampling")       return k_known_sampling.count(key) > 0;
    if (section == "classification") return k_known_classif.count(key) > 0;
    if (section == "migration")      return k_known_migration.count(key) > 0;
    if (section == "daemon")         return k_known_daemon.count(key) > 0;
    return false;
}

// Apply a single scalar key=value. Returns false if the key is unknown
// for this section; throws on parse failure (caller catches).
bool apply_scalar(Config& cfg, const std::string& section,
                  const std::string& key, const std::string& value) {
    if (!key_known(section, key)) return false;

    if (section == "tiers") {
        if (key == "hot_node")       cfg.hot_node  = std::stoi(value);
        else if (key == "cold_node") cfg.cold_node = std::stoi(value);
    } else if (section == "sampling") {
        if (key == "frequency")           cfg.frequency      = std::stoi(value);
        else if (key == "window_seconds") cfg.window_seconds = std::stoi(value);
        // "events" is array-only; ignored here.
    } else if (section == "classification") {
        if (key == "hot_percentile") cfg.hot_percentile = std::stof(value);
    } else if (section == "migration") {
        if (key == "threads")                    cfg.threads              = std::stoi(value);
        else if (key == "max_pages_per_window")  cfg.max_pages_per_window = std::stoull(value);
        else if (key == "max_idle_windows")      cfg.max_idle_windows     = std::stoi(value);
        else if (key == "region_size") {
            cfg.region_size_str   = value;
            cfg.region_size_bytes = parse_size(value);
        }
    } else if (section == "daemon") {
        if (key == "pidfile")        cfg.pidfile  = value;
        else if (key == "verbose")   cfg.verbose  = (value == "true" || value == "1");
        else if (key == "log_file")  cfg.log_file = value;
        else if (key == "perf_bin")  cfg.perf_bin = value;
    }
    return true;
}

// Apply an array key=[...]. Returns false if unknown.
bool apply_array(Config& cfg, const std::string& section,
                 const std::string& key,
                 const std::vector<std::string>& values) {
    if (!key_known(section, key)) return false;
    if (section == "sampling" && key == "events") {
        cfg.events = values;
        return true;
    }
    // Known key but not array-typed: warn.
    log_warn("Config: [%s].%s is not an array-typed key", section.c_str(), key.c_str());
    return true;
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
                if (!apply_array(cfg, section, array_key, array_values)) {
                    warn_unknown(section, array_key);
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
                if (!apply_array(cfg, section, key, items)) {
                    warn_unknown(section, key);
                }
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
            if (section.empty()) continue;
            if (!apply_scalar(cfg, section, key, value)) {
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

    // Reject events containing shell metacharacters early; otherwise
    // they would be interpolated into a /bin/sh command line.
    std::vector<std::string> safe;
    safe.reserve(cfg.events.size());
    for (const auto& ev : cfg.events) {
        if (is_valid_event_name(ev)) {
            safe.push_back(ev);
        } else {
            log_warn("Config: rejecting unsafe perf event name: '%s'", ev.c_str());
        }
    }
    cfg.events.swap(safe);

    return 0;
}
