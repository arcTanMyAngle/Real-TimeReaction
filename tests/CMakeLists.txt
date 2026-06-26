add_executable(rtr_tests
    test_database.cpp
    test_timer.cpp
    test_session.cpp
    # Production sources under test (the test TU's alone don't define these).
    ${CMAKE_SOURCE_DIR}/src/core/timer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/session.cpp
    ${CMAKE_SOURCE_DIR}/src/data/database.cpp
)
target_include_directories(rtr_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rtr_tests PRIVATE
    Catch2::Catch2WithMain SQLiteCpp ${OpenCV_LIBS} SDL2::SDL2
)

# Phase 3: two-player tests. ResultsScreen::determine_winner() is exercised
# directly, so the screen TU (and the ImGui it references) must be linked in.
target_sources(rtr_tests PRIVATE
    test_two_player.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/screens/results_screen.cpp
)
target_link_libraries(rtr_tests PRIVATE imgui_lib)

# Phase 4: analytics tests (pure computation + CSV export, no UI).
target_sources(rtr_tests PRIVATE
    test_analytics.cpp
    ${CMAKE_SOURCE_DIR}/src/data/analytics.cpp
)

include(Catch)
catch_discover_tests(rtr_tests)
