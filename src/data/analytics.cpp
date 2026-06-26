#include "analytics.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <numeric>
#include <unordered_map>

namespace Analytics {

namespace {

std::tm to_local_tm(std::time_t t) {
    std::tm out{};
#if defined(_WIN32)
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

float mean_of(const std::vector<float>& v) {
    if (v.empty()) return -1.0f;
    const double sum = std::accumulate(v.begin(), v.end(), 0.0);
    return static_cast<float>(sum / v.size());
}

} // namespace

std::vector<float> valid_rts(const std::vector<Trial>& trials, int player) {
    std::vector<float> out;
    for (const auto& t : trials) {
        if (player != 0 && t.player != player) continue;
        if (t.false_start || is_miss(t)) continue;
        out.push_back(t.reaction_time_ms);
    }
    return out;
}

Stats compute_stats(const std::vector<Trial>& trials, int player) {
    Stats s;
    std::vector<float> rts;
    for (const auto& t : trials) {
        if (player != 0 && t.player != player) continue;
        if (t.false_start) { ++s.false_starts; continue; }
        if (is_miss(t))    { ++s.misses;       continue; }
        rts.push_back(t.reaction_time_ms);
    }

    s.count = static_cast<int>(rts.size());
    if (s.count == 0)
        return s;  // everything stays at -1

    s.mean_ms = mean_of(rts);

    std::vector<float> sorted = rts;
    std::sort(sorted.begin(), sorted.end());
    s.best_ms  = sorted.front();
    s.worst_ms = sorted.back();

    const int n = s.count;
    s.median_ms = (n % 2 == 1)
                      ? sorted[n / 2]
                      : 0.5f * (sorted[n / 2 - 1] + sorted[n / 2]);

    s.p10_ms = sorted[static_cast<int>(std::floor(n * 0.10))];
    s.p90_ms = sorted[std::min(n - 1, static_cast<int>(std::floor(n * 0.90)))];

    if (n >= 2) {
        double acc = 0.0;
        for (float x : rts) {
            const double d = x - s.mean_ms;
            acc += d * d;
        }
        s.stdev_ms = static_cast<float>(std::sqrt(acc / (n - 1)));
    }

    return s;
}

std::vector<float> session_trend(
    const std::vector<std::vector<Trial>>& sessions_trials, int player) {
    std::vector<float> trend;
    trend.reserve(sessions_trials.size());
    for (const auto& session : sessions_trials)
        trend.push_back(mean_of(valid_rts(session, player)));
    return trend;
}

std::vector<SessionSummary> get_player_sessions(Database& db, int player_id) {
    const std::vector<Trial> trials = db.get_player_trials(player_id);

    std::vector<SessionSummary> out;
    std::unordered_map<int, std::size_t> index;
    for (const auto& t : trials) {
        auto it = index.find(t.session_id);
        if (it == index.end()) {
            SessionSummary ss;
            ss.session_id = t.session_id;
            const Session meta = db.get_session(t.session_id);
            ss.started_at = meta.started_at;
            ss.mode       = meta.mode;
            index[t.session_id] = out.size();
            out.push_back(std::move(ss));
            it = index.find(t.session_id);
        }
        out[it->second].trials.push_back(t);
    }
    return out;
}

std::vector<LeaderEntry> get_leaderboard(Database& db, int limit) {
    std::vector<LeaderEntry> entries;
    for (const auto& p : db.get_all_players()) {
        const Stats s = compute_stats(db.get_player_trials(p.id));
        if (s.count >= 5)
            entries.push_back({p.name, s.mean_ms, s.count, p.id});
    }
    std::sort(entries.begin(), entries.end(),
              [](const LeaderEntry& a, const LeaderEntry& b) {
                  return a.mean_ms < b.mean_ms;
              });
    if (static_cast<int>(entries.size()) > limit)
        entries.resize(limit);
    return entries;
}

fs::path export_dir() {
    fs::path dir = fs::path("data") / "exports";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

fs::path export_trials_csv(const std::vector<Trial>& trials,
                           const std::string& player_name) {
    const fs::path dir = export_dir();

    const std::time_t now = std::time(nullptr);
    const std::tm lt = to_local_tm(now);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &lt);

    std::string safe;
    for (char c : player_name)
        safe += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    if (safe.empty()) safe = "player";

    const fs::path path =
        dir / ("reaction_" + safe + "_" + std::string(ts) + ".csv");

    std::ofstream f(path);
    f << "id,session_id,trial_number,player,stimulus_type,stimulus_color,"
         "reaction_time_ms,false_start,stimulus_onset_epoch\n";
    for (const auto& t : trials) {
        std::string rt;
        if (t.false_start) {
            rt = "FALSE_START";
        } else if (is_miss(t)) {
            rt = "MISS";
        } else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1f", t.reaction_time_ms);
            rt = buf;
        }
        f << t.id << ',' << t.session_id << ',' << t.trial_number << ','
          << t.player << ',' << t.stimulus_type << ',' << t.stimulus_color << ','
          << rt << ',' << (t.false_start ? 1 : 0) << ','
          << t.stimulus_onset_epoch << '\n';
    }
    return path;
}

} // namespace Analytics
