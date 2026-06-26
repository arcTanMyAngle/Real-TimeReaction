# Phase 3: Two-Player Mode

## Goal
Extend GameScreen and ResultsScreen for two local players. Session logic already
supports two-player internally — this phase makes it visible in the UI.
Single-player must still work identically after these changes.

## Prerequisites
- Phase 2 Acceptance Criteria all passing
- `cd build && ctest --output-on-failure` fully passing

---

## Two-Player Rules (UI enforces these)
1. Trials alternate: P1 → P2 → P1 → P2 … (enforced by `GameSession`)
2. Only the **active player's key** is valid during `StimulusActive`
3. Inactive player's key during any state is silently ignored — not a false start
4. False start = **active player** presses their key during `Waiting`
5. Winner = lower average RT over all valid (non-false-start, non-miss) trials
6. Tie threshold: `|avg_p1 - avg_p2| < 5.0f` ms → "TOO CLOSE TO CALL"
7. If one player has zero valid trials → other player wins by default

---

## Deliverable 1 — Updated `src/core/session.hpp` + `session.cpp`

Add one accessor (everything else already works):
```cpp
int get_active_player() const noexcept;
```
Returns `current_player()` — already private; just expose it publicly.

Verify in `session.cpp`:
- `key_is_valid()` returns false for inactive player's key
- False start logic only fires when `key_is_valid()` returns true

No other changes to `session.cpp`.

---

## Deliverable 2 — Updated `src/ui/screens/game_screen.hpp` + `game_screen.cpp`

Add private method:
```cpp
void render_two_player_layout();
void render_player_panel(ImVec2 pos, ImVec2 size, int player,
                          const std::string& name, bool is_active,
                          const std::optional<LastResult>& last);
```

In `render()`:
```cpp
if (mode_ == "two_player")
    render_two_player_layout();
else
    render_single_player_layout();  // existing Phase 2 code, renamed
```

### Two-Player Layout
```
┌──────────────────────────────────────────────────────────────┐
│ P1 Panel  │     Camera (400×300, centered, y=60)    │P2 Panel│
│ (320px W) │     + stimulus overlay                  │(320px W│
│ full H    │     + countdown overlay                 │full H) │
│────────── │─────────────────────────────────────────│────────│
│           │  Progress bar (camera width, bottom)    │        │
└──────────────────────────────────────────────────────────────┘
```

### Player Panel content
- Player name — large, player color (`Colors::P1` or `Colors::P2`)
- "YOUR TURN" badge — `Colors::ACCENT` bg, white text — visible when `is_active`
- "WAITING"  — `Colors::MUTED` text — visible when `!is_active`
- Last RT: large number in ms, or "—" if no trial yet, or "MISS" if miss
- Key hint: `[ SPACE ]` for P1, `[ ENTER ]` for P2 — always visible

### Active Player Indicator
- Active panel: 3px `Colors::ACCENT` border on the ImGui child window
- Inactive panel: 1px `Colors::MUTED` border
- Camera feed border color matches active player color

```cpp
// Border via ImGui::PushStyleVar + child window approach:
ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, is_active ? 3.0f : 1.0f);
ImGui::PushStyleColor(ImGuiCol_Border, is_active ? Colors::ACCENT : Colors::MUTED);
ImGui::BeginChild("##p1_panel", size, true);
// ... content ...
ImGui::EndChild();
ImGui::PopStyleColor();
ImGui::PopStyleVar();
```

### False Start Flash
- `false_start_flash_timer_` float member, set to `0.6f` on false start detection
- In `render()`: if timer > 0, draw a semi-transparent red fullscreen overlay
- Text: "FALSE START — {player_name}!" centered in large font
- `update(dt)`: decrement `false_start_flash_timer_ -= dt`

```cpp
// Flash overlay:
ImDrawList* dl = ImGui::GetForegroundDrawList();
dl->AddRectFilled({0,0}, {(float)Display::W, (float)Display::H},
    IM_COL32(220, 50, 50, 120));
```

Detect false start: compare `get_last_result()` between frames — if new result has
`false_start == true`, trigger the flash.

---

## Deliverable 3 — Updated `src/ui/screens/results_screen.hpp` + `results_screen.cpp`

Add:
```cpp
std::string determine_winner() const;
// Returns "P1" | "P2" | "TIE" | "NO_DATA"

void render_winner_banner();
void render_two_player_stats();
```

### Two-Player Results Layout
```
┌──────────────────────────────────────────────┐
│            SESSION RESULTS                   │
│──────────────────────────────────────────────│
│  ┌── PLAYER 1 ──┐    ┌── PLAYER 2 ──┐       │
│  │ Avg:  210 ms │    │ Avg:  245 ms │       │
│  │ Best: 180 ms │    │ Best: 201 ms │       │
│  │ Worst:310 ms │    │ Worst:330 ms │       │
│  │ Misses: 1    │    │ Misses: 0    │       │
│  └──────────────┘    └──────────────┘       │
│──────────────────────────────────────────────│
│       *** PLAYER 1 WINS ***                  │
│       35 ms faster on average                │
│──────────────────────────────────────────────│
│  Trial log (P1 rows tinted P1 color,         │
│             P2 rows tinted P2 color)         │
│  [scrollable — mouse wheel or UP/DOWN]       │
│──────────────────────────────────────────────│
│  [ PLAY AGAIN ]     [ MAIN MENU ]            │
└──────────────────────────────────────────────┘
```

Winner banner colors:
- P1 wins → `Colors::P1`
- P2 wins → `Colors::P2`
- TIE     → `Colors::WARNING`
- NO_DATA → `Colors::MUTED`

Trial log row background tinting:
```cpp
ImVec4 tint = (trial.player == 1) ? Colors::P1 : Colors::P2;
tint.w = 0.15f;  // subtle background
ImGui::PushStyleColor(ImGuiCol_ChildBg, tint);
```

`determine_winner()`:
```cpp
std::string ResultsScreen::determine_winner() const {
    auto avg = [&](int player) -> float {
        float sum = 0; int n = 0;
        for (auto& t : trials_)
            if (t.player == player && !t.false_start && !is_miss(t))
                { sum += t.reaction_time_ms; ++n; }
        return n > 0 ? sum / n : -1.0f;
    };
    float a1 = avg(1), a2 = avg(2);
    if (a1 < 0 && a2 < 0) return "NO_DATA";
    if (a1 < 0) return "P2";
    if (a2 < 0) return "P1";
    if (std::abs(a1 - a2) < 5.0f) return "TIE";
    return (a1 < a2) ? "P1" : "P2";
}
```

---

## Deliverable 4 — `tests/test_two_player.cpp`

Add to `tests/CMakeLists.txt`:
```cmake
target_sources(rtr_tests PRIVATE test_two_player.cpp)
```

```cpp
#include <catch2/catch_test_macros.hpp>
#include <SDL2/SDL_keycode.h>
#include "core/session.hpp"
#include "data/database.hpp"

// Helper: advance session time by ticking with empty keys
static SessionState advance(GameSession& s, float seconds,
                             float step = 0.016f) {
    float t = 0;
    SessionState st = s.get_state();
    while (t < seconds) {
        st = s.tick(t, {});
        t += step;
    }
    return st;
}

TEST_CASE("Two-player: trials alternate P1/P2") {
    Database db(":memory:"); db.create_schema();
    GameSession s("two_player", {"Alice","Bob"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    advance(s, GameSession::ISI_MAX_S + 0.1f);
    REQUIRE(s.get_active_player() == 1);  // first trial is P1
}

TEST_CASE("Two-player: inactive player key ignored during Waiting") {
    Database db(":memory:"); db.create_schema();
    GameSession s("two_player", {"Alice","Bob"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    auto state_before = s.get_state();  // Waiting
    // P2 key during P1's Waiting — should be ignored
    s.tick(GameSession::COUNTDOWN_S + 0.2f, {SDLK_RETURN});
    REQUIRE(s.get_state() == SessionState::Waiting);
    bool any_false_start = false;
    for (auto& t : s.get_results())
        if (t.false_start) any_false_start = true;
    REQUIRE_FALSE(any_false_start);
}

TEST_CASE("Two-player: active player false start recorded") {
    Database db(":memory:"); db.create_schema();
    GameSession s("two_player", {"Alice","Bob"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    s.tick(GameSession::COUNTDOWN_S + 0.2f, {SDLK_SPACE});  // P1 false start
    bool found = false;
    for (auto& t : s.get_results())
        if (t.false_start) found = true;
    REQUIRE(found);
}

TEST_CASE("Two-player: inactive player key ignored during StimulusActive") {
    Database db(":memory:"); db.create_schema();
    GameSession s("two_player", {"Alice","Bob"}, db);
    s.start();
    advance(s, GameSession::COUNTDOWN_S + 0.1f);
    advance(s, GameSession::ISI_MAX_S + 0.1f);  // → StimulusActive (P1)
    REQUIRE(s.get_state() == SessionState::StimulusActive);
    s.tick(9999.0f, {SDLK_RETURN});  // P2 key during P1 trial
    REQUIRE(s.get_state() == SessionState::StimulusActive);  // unchanged
}

TEST_CASE("Winner determination: basic") {
    // Build trial list manually for ResultsScreen test
    std::vector<Trial> trials;
    for (int i = 0; i < 5; ++i) {
        Trial t; t.player=1; t.reaction_time_ms=200.0f; t.false_start=false;
        trials.push_back(t);
    }
    for (int i = 0; i < 5; ++i) {
        Trial t; t.player=2; t.reaction_time_ms=250.0f; t.false_start=false;
        trials.push_back(t);
    }
    ResultsScreen rs(trials, {"Alice","Bob"});
    REQUIRE(rs.determine_winner() == "P1");
}

TEST_CASE("Winner determination: tie within 5ms") {
    std::vector<Trial> trials;
    for (int i=0; i<5; ++i) {
        Trial t1; t1.player=1; t1.reaction_time_ms=200.0f; t1.false_start=false;
        Trial t2; t2.player=2; t2.reaction_time_ms=203.0f; t2.false_start=false;
        trials.push_back(t1); trials.push_back(t2);
    }
    ResultsScreen rs(trials, {"A","B"});
    REQUIRE(rs.determine_winner() == "TIE");
}

TEST_CASE("Winner determination: all misses for P1") {
    std::vector<Trial> trials;
    for (int i=0; i<5; ++i) {
        Trial t1; t1.player=1; t1.reaction_time_ms=-1.0f; t1.false_start=false;
        Trial t2; t2.player=2; t2.reaction_time_ms=250.0f; t2.false_start=false;
        trials.push_back(t1); trials.push_back(t2);
    }
    ResultsScreen rs(trials, {"A","B"});
    REQUIRE(rs.determine_winner() == "P2");
}
```

Note: `determine_winner()` must be `public` for tests to access it.

---

## Acceptance Criteria
- [ ] Menu "TWO PLAYER" button leads to two-name input
- [ ] Both player panels visible in game screen; active panel has accent border
- [ ] Camera feed border matches active player color
- [ ] SPACE only registers for P1 trials; ENTER only for P2 trials
- [ ] Inactive player key during any state has no effect
- [ ] False start flash (red overlay, 0.6s) appears when active player presses during Waiting
- [ ] Trials alternate P1→P2→P1→P2
- [ ] Results screen: both stats panels visible; winner banner displayed
- [ ] TIE banner shown when averages within 5ms
- [ ] Trial log rows tinted by player color
- [ ] Single-player mode behaviour unchanged from Phase 2
- [ ] `cd build && ctest --output-on-failure` — all tests pass

## Out of Scope
- Network / remote multiplayer
- Motion-zone split-screen detection
- Analytics screen (Phase 4)
- Sound
