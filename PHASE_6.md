# Phase 6: Game Modes, Session Rank & Streak

## Goal
Four distinct game modes — Classic, Race, Blitz, Survival — with mode-appropriate
HUD, live streak counter, and a large session rank (S/A/B/C/D) on the results screen.
This is what makes reActivation a game rather than a tool.

## Prerequisites
- Phase 5 Acceptance Criteria all passing
- `cd build && ctest --output-on-failure` fully passing

---

## Mode Definitions

| Mode | Players | End Condition | Win By |
|---|---|---|---|
| **Classic** | 1 or 2 (alternating) | Fixed trials | Lowest avg RT |
| **Race** | 2 (simultaneous) | Fixed rounds | Most rounds won |
| **Blitz** | 1 or 2 (alternating) | 30s timer | Most valid presses |
| **Survival** | 1 or 2 (alternating) | Lives reach 0 | Trials survived |

---

## Deliverable 1 — Updated `src/data/models.hpp`

Add fields to existing structs:
```cpp
struct Session {
    // ... existing fields ...
    std::string game_mode = "classic"; // "classic"|"race"|"blitz"|"survival"
};

struct Trial {
    // ... existing fields ...
    int round_winner = 0;   // 0=no contest, 1=P1, 2=P2 (Race mode only)
    int streak_at_time = 0; // live streak count when this trial completed
};
```

---

## Deliverable 2 — DB Schema Migration in `database.cpp`

Add to the end of `create_schema()` — SQLite ADD COLUMN is safe if wrapped:
```cpp
auto try_alter = [&](const char* sql) {
    try { db_.exec(sql); } catch (...) {}  // silently skip if column exists
};
try_alter("ALTER TABLE sessions ADD COLUMN game_mode TEXT NOT NULL DEFAULT 'classic'");
try_alter("ALTER TABLE trials   ADD COLUMN round_winner INTEGER NOT NULL DEFAULT 0");
try_alter("ALTER TABLE trials   ADD COLUMN streak_at_time INTEGER NOT NULL DEFAULT 0");
```

Update `insert_session()` and `insert_trial()` to bind the new fields.

---

## Deliverable 3 — Updated `src/core/session.hpp` + `session.cpp`

### GameMode enum
```cpp
enum class GameMode { Classic, Race, Blitz, Survival };

inline GameMode mode_from_string(const std::string& s) {
    if (s == "race")     return GameMode::Race;
    if (s == "blitz")    return GameMode::Blitz;
    if (s == "survival") return GameMode::Survival;
    return GameMode::Classic;
}
```

### New GameSession members
```cpp
class GameSession {
public:
    // New constants
    static constexpr float BLITZ_DURATION_S  = 30.0f;
    static constexpr float BLITZ_ISI_MIN_S   = 0.5f;
    static constexpr float BLITZ_ISI_MAX_S   = 1.5f;
    static constexpr int   SURVIVAL_LIVES    = 3;
    static constexpr float STREAK_THRESHOLD_MS = 300.0f;

    // New accessors
    GameMode get_mode()       const noexcept { return mode_; }
    int  get_lives(int player)  const noexcept;  // player: 1 or 2
    float get_time_remaining()  const noexcept;  // Blitz only
    int  get_current_streak()  const noexcept;
    int  get_max_streak()      const noexcept;
    int  get_rounds_won(int player) const noexcept;  // Race only

private:
    GameMode mode_ = GameMode::Classic;
    int      lives_[2]      = {SURVIVAL_LIVES, SURVIVAL_LIVES};
    float    time_remaining_ = BLITZ_DURATION_S;
    int      current_streak_ = 0;
    int      max_streak_    = 0;
    int      rounds_won_[2] = {0, 0};
};
```

### Mode-specific tick() changes

**Race mode — StimulusActive:**
```cpp
// Both SDLK_SPACE and SDLK_RETURN are valid simultaneously
// First valid press wins the round
// Second press after first is ignored
// Record round_winner on the trial
for (SDL_Keycode k : keys) {
    if (k == SDLK_SPACE && !round_settled_) {
        current_trial_->round_winner = 1;
        rounds_won_[0]++;
        record_response(elapsed_ms);
        round_settled_ = true;
    } else if (k == SDLK_RETURN && !round_settled_) {
        current_trial_->round_winner = 2;
        rounds_won_[1]++;
        record_response(elapsed_ms);
        round_settled_ = true;
    }
}
if (round_settled_) enter_collecting(current_time_s);
if (elapsed_ms >= TIMEOUT_MS && !round_settled_) {
    record_timeout(); enter_collecting(current_time_s);
}
// In Race mode: current_player() always returns 1 for trial sequencing
// but both keys valid — reset round_settled_ = false on enter_stimulus()
```

**Blitz mode — tick():**
```cpp
// Decrement timer during any non-Complete state
if (mode_ == GameMode::Blitz && state_ != SessionState::Complete) {
    time_remaining_ -= (current_time_s - last_tick_time_s_);
    if (time_remaining_ <= 0.0f) { complete(); return state_; }
}
// ISI uses BLITZ_ISI_MIN/MAX instead of standard ISI
// No trial limit — trial_counter_ has no ceiling in Blitz mode
```

**Survival mode — enter_collecting():**
```cpp
if (mode_ == GameMode::Survival) {
    bool lost_life = is_miss(*latest_trial) || latest_trial->false_start;
    if (lost_life) {
        int p = current_player() - 1;
        lives_[p] = std::max(0, lives_[p] - 1);
        if (lives_[p] == 0) { complete(); return; }
    }
}
```

**Streak — update after every valid response:**
```cpp
void GameSession::update_streak(float rt_ms, bool false_start, bool miss) {
    if (!false_start && !miss && rt_ms <= STREAK_THRESHOLD_MS) {
        ++current_streak_;
        max_streak_ = std::max(max_streak_, current_streak_);
    } else {
        current_streak_ = 0;
    }
    if (current_trial_) current_trial_->streak_at_time = current_streak_;
}
// Call update_streak() from record_response(), record_timeout(), record_false_start()
```

---

## Deliverable 4 — Updated `src/ui/screens/menu_screen.hpp/cpp`

### New layout: Mode Selection Grid
```
┌────────────────────────────────────────────────┐
│          reActivation                          │
│       Reaction Time Trainer                    │
│                                                │
│  [  CLASSIC  ] [   RACE   ] [ BLITZ ] [SURVIVE]│
│   (highlighted = selected)                     │
│                                                │
│  Player 1: [input]                            │
│  (Race forces two-player; others toggle)       │
│  Player 2: [input]   (if two-player)           │
│                                                │
│              [ START ]                         │
│              [ Analytics ]                     │
└────────────────────────────────────────────────┘
```

Mode button behavior:
- Selected mode: `Colors::ACCENT` background, dark text
- Unselected: `Colors::PANEL` background, muted text
- **Race** selected: show P2 input unconditionally (requires 2 players)
- **Classic/Blitz/Survival**: show `[Single] [Two Player]` sub-toggle

Add `game_mode` to `ScreenResult`:
```cpp
struct ScreenResult {
    // ... existing ...
    std::string game_mode = "classic";
};
```

---

## Deliverable 5 — Updated `src/ui/screens/game_screen.hpp/cpp`

Add mode-specific HUD elements. All in `render_hud_panel()` based on `session_.get_mode()`:

### Streak Counter (all modes)
```cpp
// Display live in HUD panel
if (session_.get_current_streak() >= 2) {
    // "x{N} STREAK" in Colors::WARNING (yellow)
    // Pulse scale via sin wave when streak >= 5
}
```

### Blitz: Countdown Timer
```cpp
float t = session_.get_time_remaining();
ImVec4 timer_col = (t > 15.0f) ? Colors::SUCCESS
                 : (t > 5.0f)  ? Colors::WARNING
                 :                Colors::DANGER;
// Scale: pulsate when t < 5.0f using SetWindowFontScale + sin
// Display: "29.4s" large, centered at top of HUD panel
```

### Survival: Lives Display
```cpp
void render_lives(ImVec2 pos, int lives, ImVec4 color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < GameSession::SURVIVAL_LIVES; ++i) {
        ImVec2 c = {pos.x + i * 32.0f, pos.y};
        bool alive = (i < lives);
        dl->AddCircleFilled(c, 11.0f,
            alive ? ImGui::ColorConvertFloat4ToU32(color)
                  : IM_COL32(50, 50, 55, 255));
        dl->AddCircle(c, 11.0f, IM_COL32(100, 100, 110, 200), 0, 1.5f);
    }
}
// Call for P1 (Colors::P1) and P2 (Colors::P2) if two_player
```

### Race: Round Score
```cpp
// Show "P1: {N} wins   P2: {N} wins" prominently in HUD
// Active player highlighted in their color
```

### Streak audio trigger (in game_screen):
```cpp
// In update(), after session_.tick():
int streak = session_.get_current_streak();
if (streak > 0 && streak > prev_streak_ && streak % 3 == 0) {
    audio_.play(AudioEngine::Sound::Hit);  // extra hit sound on streak milestone
}
prev_streak_ = streak;
```

---

## Deliverable 6 — Updated `src/ui/screens/results_screen.hpp/cpp`

### Session Rank
```cpp
char compute_rank(float mean_ms, int misses, int total) const {
    if (mean_ms < 0.0f || total == 0) return 'D';
    // Penalize misses: each miss adds 40ms to effective average
    float adj = mean_ms + (misses * 40.0f);
    if (adj < 200.0f) return 'S';
    if (adj < 250.0f) return 'A';
    if (adj < 300.0f) return 'B';
    if (adj < 350.0f) return 'C';
    return 'D';
}

ImVec4 rank_color(char r) const {
    switch(r) {
        case 'S': return ImVec4{1.0f, 0.84f, 0.0f, 1.0f}; // gold
        case 'A': return Colors::SUCCESS;
        case 'B': return Colors::ACCENT;
        case 'C': return Colors::WARNING;
        default:  return Colors::DANGER;
    }
}
```

### Rank display — large, centered, top of results screen:
```cpp
void render_rank_display(char rank) {
    // e.g. "[  S  ]" or "[  B  ]"
    // Use SetWindowFontScale(4.0f) for the rank letter
    // Surrounding brackets at 2.0f scale
    // Color = rank_color(rank)
    // Subtitle below: rank description text
    //   S="Superhuman", A="Elite", B="Solid", C="Average", D="Keep Practicing"
}
```

### Streak display (below rank):
```cpp
ImGui::Text("Max Streak: %d  (under %.0f ms)", max_streak_, GameSession::STREAK_THRESHOLD_MS);
// Color: SUCCESS if max_streak >= 5, ACCENT if >= 3, MUTED if < 3
```

### Mode-specific results header:

**Race**: show round wins per player: "Alice  6 — 4  Bob" with winner name in ACCENT
**Blitz**: show valid press count + total trials attempted; score is valid count
**Survival**: show "Survived X trials" + lives remaining indicator
**Classic**: existing stats layout unchanged

### Results screen must accept `game_mode` from `GameScreen`:
```cpp
// Add to ResultsScreen constructor:
ResultsScreen(const std::vector<Trial>& trials,
              const std::vector<std::string>& player_names,
              GameMode mode,
              int max_streak);
```

`GameScreen` stores `max_streak_` from `session_.get_max_streak()` before transitioning.

---

## Deliverable 7 — `tests/test_game_modes.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <SDL2/SDL_keycode.h>
#include "core/session.hpp"
#include "data/database.hpp"

// Helpers
static Database make_db() {
    Database db(":memory:"); db.create_schema(); return db;
}
static SessionState advance(GameSession& s, float secs, float step = 0.016f) {
    SessionState st = s.get_state();
    for (float t = 0; t < secs; t += step) st = s.tick(t, {});
    return st;
}

// ── Race ─────────────────────────────────────────────────────────────────────

TEST_CASE("Race: both keys valid simultaneously in StimulusActive") {
    auto db = make_db();
    GameSession s("race", {"A","B"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    advance(s, GameSession::ISI_MAX_S + 0.1f);
    REQUIRE(s.get_state() == SessionState::StimulusActive);
    // P2 key should win the round
    s.tick(100.0f, {SDLK_RETURN});
    REQUIRE(s.get_state() == SessionState::Collecting);
    REQUIRE(s.get_rounds_won(2) == 1);
    REQUIRE(s.get_rounds_won(1) == 0);
}

TEST_CASE("Race: second key after first is ignored") {
    auto db = make_db();
    GameSession s("race", {"A","B"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    advance(s, GameSession::ISI_MAX_S + 0.1f);
    s.tick(100.0f, {SDLK_SPACE});   // P1 wins
    REQUIRE(s.get_rounds_won(1) == 1);
    // Simulate second key in same frame (should be no-op)
    s.tick(100.0f, {SDLK_RETURN});  // already in Collecting
    REQUIRE(s.get_rounds_won(2) == 0);
}

// ── Blitz ────────────────────────────────────────────────────────────────────

TEST_CASE("Blitz: transitions to Complete when time runs out") {
    auto db = make_db();
    GameSession s("blitz", {"A"}, db);
    s.start();
    // Advance past countdown
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    // Advance 31 seconds (past BLITZ_DURATION_S=30)
    SessionState st = advance(s, 31.0f, 0.1f);
    REQUIRE(st == SessionState::Complete);
}

TEST_CASE("Blitz: time_remaining decrements") {
    auto db = make_db();
    GameSession s("blitz", {"A"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 1.0f, 0.1f);
    REQUIRE(s.get_time_remaining() < GameSession::BLITZ_DURATION_S);
}

// ── Survival ─────────────────────────────────────────────────────────────────

TEST_CASE("Survival: miss removes a life") {
    auto db = make_db();
    GameSession s("survival", {"A"}, db);
    s.start();
    REQUIRE(s.get_lives(1) == GameSession::SURVIVAL_LIVES);
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    advance(s, GameSession::ISI_MAX_S + 0.1f);
    // Timeout without pressing
    advance(s, GameSession::TIMEOUT_MS / 1000.0f + 0.1f, 0.016f);
    REQUIRE(s.get_lives(1) == GameSession::SURVIVAL_LIVES - 1);
}

TEST_CASE("Survival: Complete when lives reach 0") {
    auto db = make_db();
    GameSession s("survival", {"A"}, db);
    s.start();
    // Force 3 misses
    for (int i = 0; i < GameSession::SURVIVAL_LIVES; ++i) {
        advance(s, GameSession::COUNTDOWN_S + 0.1f);
        advance(s, GameSession::ISI_MAX_S + 0.1f);
        advance(s, GameSession::TIMEOUT_MS / 1000.0f + 0.2f, 0.016f);
        advance(s, GameSession::FEEDBACK_MS / 1000.0f + 0.1f, 0.016f);
    }
    REQUIRE(s.get_state() == SessionState::Complete);
}

// ── Streak ───────────────────────────────────────────────────────────────────

TEST_CASE("Streak: increments on fast valid press") {
    auto db = make_db();
    GameSession s("classic", {"A"}, db);
    s.start();
    // Complete one fast trial
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    advance(s, GameSession::ISI_MAX_S + 0.1f);
    s.tick(200.0f, {SDLK_SPACE});  // 200ms → fast
    REQUIRE(s.get_current_streak() == 1);
    REQUIRE(s.get_max_streak() == 1);
}

TEST_CASE("Streak: resets on miss") {
    auto db = make_db();
    GameSession s("classic", {"A"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    advance(s, GameSession::ISI_MAX_S + 0.1f);
    s.tick(200.0f, {SDLK_SPACE});  // streak = 1
    advance(s, GameSession::FEEDBACK_MS / 1000.0f + 0.1f, 0.016f);
    advance(s, GameSession::ISI_MAX_S + 0.1f);
    // timeout
    advance(s, GameSession::TIMEOUT_MS / 1000.0f + 0.1f, 0.016f);
    REQUIRE(s.get_current_streak() == 0);
    REQUIRE(s.get_max_streak() == 1);  // max preserved
}

// ── Session Rank ─────────────────────────────────────────────────────────────

TEST_CASE("Rank: S for mean < 200ms with no misses") {
    ResultsScreen rs({}, {"A"}, GameMode::Classic, 0);
    REQUIRE(rs.compute_rank(190.0f, 0, 10) == 'S');
}
TEST_CASE("Rank: penalizes misses") {
    ResultsScreen rs({}, {"A"}, GameMode::Classic, 0);
    // 190ms mean + 2 misses (2*40=80) = 270ms adjusted → B
    REQUIRE(rs.compute_rank(190.0f, 2, 10) == 'B');
}
TEST_CASE("Rank: D for empty results") {
    ResultsScreen rs({}, {"A"}, GameMode::Classic, 0);
    REQUIRE(rs.compute_rank(-1.0f, 0, 0) == 'D');
}
```

---

## Acceptance Criteria

### Menu
- [ ] Four mode buttons: CLASSIC / RACE / BLITZ / SURVIVAL
- [ ] Race auto-shows P2 input; others show single/two-player toggle
- [ ] Selected mode highlighted in ACCENT color

### Race Mode
- [ ] Both players' keys valid simultaneously in StimulusActive
- [ ] First press wins the round; second press ignored
- [ ] HUD shows round score: "P1: N wins  P2: N wins"
- [ ] Results screen shows round score as primary stat + winner banner

### Blitz Mode
- [ ] 30 second countdown visible in HUD; pulses red under 5s
- [ ] Session ends automatically when time reaches 0
- [ ] Results show valid press count as score

### Survival Mode
- [ ] Three life circles visible in HUD; dim when lost
- [ ] Miss or false start removes one life
- [ ] Session ends when lives reach 0
- [ ] Results show "Survived X trials"

### Streak & Rank
- [ ] Live streak counter visible in HUD during all modes
- [ ] "xN STREAK" pulses for streak >= 5
- [ ] Session rank letter displayed large on results screen
- [ ] Rank color: S=gold, A=green, B=teal, C=yellow, D=red
- [ ] Rank subtitle text visible (e.g. "ELITE")
- [ ] Max streak shown on results screen

### Regression
- [ ] Phase 5 audio and effects still working in all modes
- [ ] Classic single-player and two-player unchanged
- [ ] Analytics screen still navigable; CSV export still works
- [ ] `cd build && ctest --output-on-failure` — all tests pass

## Out of Scope
- Ghost Race (race against personal best session replay)
- Online / networked multiplayer
- Mouse/gamepad input
- Volume settings UI
- Peripheral stimulus placement