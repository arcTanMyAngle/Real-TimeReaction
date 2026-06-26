#pragma once
#include <string>
#include <vector>
#include <SQLiteCpp/SQLiteCpp.h>
#include "models.hpp"

class Database {
public:
    explicit Database(const std::string& path = "data/reaction.db");

    void create_schema();   // idempotent

    // Players
    int  insert_player(const std::string& name);
    int  get_or_create_player(const std::string& name);
    std::vector<Player> get_all_players();

    // Sessions
    int     insert_session(const Session& s);
    void    close_session(int session_id);
    Session get_session(int session_id);

    // Trials
    int  insert_trial(const Trial& t);
    std::vector<Trial> get_session_trials(int session_id);
    std::vector<Trial> get_player_trials(int player_id);

    static std::string now_iso();

private:
    SQLite::Database db_;
    void ensure_dir(const std::string& path);
};
