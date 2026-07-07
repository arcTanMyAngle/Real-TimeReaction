#include "database.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace {

// Create the parent directory for a DB file path (no-op for ":memory:" and for
// paths with no directory component). Returns the path unchanged so it can be
// used inside the member-initializer list before db_ is constructed.
const std::string& prepare_db_path(const std::string& path) {
    if (path == ":memory:")
        return path;
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    return path;
}

// Thread-safe-ish UTC conversion across platforms.
std::tm to_utc_tm(std::time_t t) {
    std::tm out{};
#if defined(_WIN32)
    gmtime_s(&out, &t);
#else
    gmtime_r(&t, &out);
#endif
    return out;
}

} // namespace

Database::Database(const std::string& path)
    : db_(prepare_db_path(path),
          SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
    db_.exec("PRAGMA journal_mode=WAL;");
    db_.exec("PRAGMA foreign_keys=ON;");
}

void Database::ensure_dir(const std::string& path) {
    prepare_db_path(path);
}

void Database::create_schema() {
    db_.exec(
        "CREATE TABLE IF NOT EXISTS players ("
        "    id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name       TEXT NOT NULL UNIQUE,"
        "    created_at TEXT NOT NULL"
        ");");

    db_.exec(
        "CREATE TABLE IF NOT EXISTS sessions ("
        "    id           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    player1_id   INTEGER NOT NULL REFERENCES players(id),"
        "    player2_id   INTEGER          REFERENCES players(id),"
        "    mode         TEXT    NOT NULL CHECK(mode IN ('single','two_player')),"
        "    started_at   TEXT    NOT NULL,"
        "    completed_at TEXT,"
        "    notes        TEXT    NOT NULL DEFAULT ''"
        ");");

    db_.exec(
        "CREATE TABLE IF NOT EXISTS settings ("
        "    key   TEXT PRIMARY KEY,"
        "    value TEXT NOT NULL"
        ");");

    db_.exec(
        "CREATE TABLE IF NOT EXISTS trials ("
        "    id                   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    session_id           INTEGER NOT NULL REFERENCES sessions(id),"
        "    trial_number         INTEGER NOT NULL,"
        "    stimulus_type        TEXT    NOT NULL,"
        "    stimulus_color       TEXT    NOT NULL,"
        "    player               INTEGER NOT NULL CHECK(player IN (1,2)),"
        "    stimulus_onset_epoch REAL    NOT NULL,"
        "    reaction_time_ms     REAL    NOT NULL DEFAULT -1,"
        "    false_start          INTEGER NOT NULL DEFAULT 0,"
        "    response_epoch       REAL    NOT NULL DEFAULT 0"
        ");");

    // Phase 6 migration — additive columns. Safe to run on an existing DB:
    // ALTER ... ADD COLUMN throws if the column already exists, so swallow it.
    auto try_alter = [&](const char* sql) {
        try { db_.exec(sql); } catch (...) {}
    };
    try_alter("ALTER TABLE sessions ADD COLUMN game_mode TEXT NOT NULL DEFAULT 'classic'");
    try_alter("ALTER TABLE trials   ADD COLUMN round_winner INTEGER NOT NULL DEFAULT 0");
    try_alter("ALTER TABLE trials   ADD COLUMN streak_at_time INTEGER NOT NULL DEFAULT 0");
}

// ── Players ──────────────────────────────────────────────────────────────────
int Database::insert_player(const std::string& name) {
    SQLite::Statement stmt(
        db_, "INSERT INTO players (name, created_at) VALUES (?, ?)");
    stmt.bind(1, name);
    stmt.bind(2, now_iso());
    stmt.exec();
    return static_cast<int>(db_.getLastInsertRowid());
}

int Database::get_or_create_player(const std::string& name) {
    {
        SQLite::Statement sel(db_, "SELECT id FROM players WHERE name = ?");
        sel.bind(1, name);
        if (sel.executeStep())
            return sel.getColumn(0).getInt();
    }
    return insert_player(name);
}

std::vector<Player> Database::get_all_players() {
    std::vector<Player> out;
    SQLite::Statement stmt(
        db_, "SELECT id, name, created_at FROM players ORDER BY id");
    while (stmt.executeStep()) {
        Player p;
        p.id         = stmt.getColumn(0).getInt();
        p.name       = stmt.getColumn(1).getString();
        p.created_at = stmt.getColumn(2).getString();
        out.push_back(std::move(p));
    }
    return out;
}

// ── Sessions ─────────────────────────────────────────────────────────────────
int Database::insert_session(const Session& s) {
    SQLite::Statement stmt(
        db_,
        "INSERT INTO sessions (player1_id, player2_id, mode, started_at, "
        "completed_at, notes, game_mode) VALUES (?, ?, ?, ?, ?, ?, ?)");
    stmt.bind(1, s.player1_id);
    if (s.player2_id > 0)
        stmt.bind(2, s.player2_id);
    else
        stmt.bind(2);  // NULL for single player
    stmt.bind(3, s.mode);
    stmt.bind(4, s.started_at.empty() ? now_iso() : s.started_at);
    if (s.completed_at.empty())
        stmt.bind(5);  // NULL
    else
        stmt.bind(5, s.completed_at);
    stmt.bind(6, s.notes);
    stmt.bind(7, s.game_mode.empty() ? "classic" : s.game_mode);
    stmt.exec();
    return static_cast<int>(db_.getLastInsertRowid());
}

void Database::close_session(int session_id) {
    SQLite::Statement stmt(
        db_, "UPDATE sessions SET completed_at = ? WHERE id = ?");
    stmt.bind(1, now_iso());
    stmt.bind(2, session_id);
    stmt.exec();
}

Session Database::get_session(int session_id) {
    Session s;
    SQLite::Statement stmt(
        db_,
        "SELECT id, player1_id, player2_id, mode, started_at, completed_at, "
        "notes, game_mode FROM sessions WHERE id = ?");
    stmt.bind(1, session_id);
    if (stmt.executeStep()) {
        s.id           = stmt.getColumn(0).getInt();
        s.player1_id   = stmt.getColumn(1).getInt();
        s.player2_id   = stmt.getColumn(2).isNull() ? 0 : stmt.getColumn(2).getInt();
        s.mode         = stmt.getColumn(3).getString();
        s.started_at   = stmt.getColumn(4).getString();
        s.completed_at = stmt.getColumn(5).isNull() ? "" : stmt.getColumn(5).getString();
        s.notes        = stmt.getColumn(6).getString();
        s.game_mode    = stmt.getColumn(7).getString();
    }
    return s;
}

// ── Trials ───────────────────────────────────────────────────────────────────
int Database::insert_trial(const Trial& t) {
    SQLite::Statement stmt(
        db_,
        "INSERT INTO trials (session_id, trial_number, stimulus_type, "
        "stimulus_color, player, stimulus_onset_epoch, reaction_time_ms, "
        "false_start, response_epoch, round_winner, streak_at_time) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    stmt.bind(1, t.session_id);
    stmt.bind(2, t.trial_number);
    stmt.bind(3, t.stimulus_type);
    stmt.bind(4, t.stimulus_color);
    stmt.bind(5, t.player);
    stmt.bind(6, t.stimulus_onset_epoch);
    stmt.bind(7, static_cast<double>(t.reaction_time_ms));
    stmt.bind(8, t.false_start ? 1 : 0);
    stmt.bind(9, t.response_epoch);
    stmt.bind(10, t.round_winner);
    stmt.bind(11, t.streak_at_time);
    stmt.exec();
    return static_cast<int>(db_.getLastInsertRowid());
}

namespace {

Trial read_trial(SQLite::Statement& stmt) {
    Trial t;
    t.id                   = stmt.getColumn(0).getInt();
    t.session_id           = stmt.getColumn(1).getInt();
    t.trial_number         = stmt.getColumn(2).getInt();
    t.stimulus_type        = stmt.getColumn(3).getString();
    t.stimulus_color       = stmt.getColumn(4).getString();
    t.player               = stmt.getColumn(5).getInt();
    t.stimulus_onset_epoch = stmt.getColumn(6).getDouble();
    t.reaction_time_ms     = static_cast<float>(stmt.getColumn(7).getDouble());
    t.false_start          = stmt.getColumn(8).getInt() != 0;
    t.response_epoch       = stmt.getColumn(9).getDouble();
    t.round_winner         = stmt.getColumn(10).getInt();
    t.streak_at_time       = stmt.getColumn(11).getInt();
    return t;
}

// Qualified with the "t" alias so the column list is unambiguous when trials
// is joined with sessions (both tables have an "id" column).
constexpr const char* kTrialCols =
    "t.id, t.session_id, t.trial_number, t.stimulus_type, t.stimulus_color, "
    "t.player, t.stimulus_onset_epoch, t.reaction_time_ms, t.false_start, "
    "t.response_epoch, t.round_winner, t.streak_at_time";

} // namespace

std::vector<Trial> Database::get_session_trials(int session_id) {
    std::vector<Trial> out;
    SQLite::Statement stmt(
        db_, std::string("SELECT ") + kTrialCols +
                 " FROM trials t WHERE t.session_id = ? ORDER BY t.trial_number");
    stmt.bind(1, session_id);
    while (stmt.executeStep())
        out.push_back(read_trial(stmt));
    return out;
}

std::vector<Trial> Database::get_player_trials(int player_id) {
    std::vector<Trial> out;
    // Trials whose session lists this player; map session.player slot → trial.player.
    SQLite::Statement stmt(
        db_,
        std::string("SELECT ") + kTrialCols +
            " FROM trials t JOIN sessions s ON t.session_id = s.id "
            "WHERE (t.player = 1 AND s.player1_id = ?) "
            "   OR (t.player = 2 AND s.player2_id = ?) "
            "ORDER BY t.id");
    stmt.bind(1, player_id);
    stmt.bind(2, player_id);
    while (stmt.executeStep())
        out.push_back(read_trial(stmt));
    return out;
}

// ── Settings (persisted user/session state) ──────────────────────────────────
std::string Database::get_setting(const std::string& key,
                                  const std::string& def) {
    SQLite::Statement stmt(db_, "SELECT value FROM settings WHERE key = ?");
    stmt.bind(1, key);
    if (stmt.executeStep())
        return stmt.getColumn(0).getString();
    return def;
}

void Database::set_setting(const std::string& key, const std::string& value) {
    SQLite::Statement stmt(
        db_, "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    stmt.bind(1, key);
    stmt.bind(2, value);
    stmt.exec();
}

float Database::get_personal_best(int player_id) {
    SQLite::Statement stmt(
        db_,
        "SELECT MIN(reaction_time_ms) FROM trials "
        "WHERE session_id IN (SELECT id FROM sessions "
        "                     WHERE player1_id = ? OR player2_id = ?) "
        "  AND false_start = 0 AND reaction_time_ms >= 0");
    stmt.bind(1, player_id);
    stmt.bind(2, player_id);
    if (stmt.executeStep() && !stmt.getColumn(0).isNull())
        return static_cast<float>(stmt.getColumn(0).getDouble());
    return -1.0f;
}

std::string Database::now_iso() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    const std::tm utc = to_utc_tm(t);
    std::ostringstream os;
    os << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}
