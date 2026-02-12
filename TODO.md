  1. minesweeper.h + minesweeper.c
     Summary: app bootstrap, dependency wiring, global app state, scene/view lifecycle.
     Fix/improve:

  - ensure_solvable_board is effectively not initialized/persisted at startup, and game alloc hardcodes
    false (minesweeper.c:78), so “Ensure Solvable” state is not honored on app launch.
  - Default settings path sets width/height/difficulty/feedback/wrap but not ensure_solvable_board
    (minesweeper.c:52).
  - Dead field: ensure_map_solvable appears unused (minesweeper.h:60).
  - Add allocation/failure guards around malloc/service allocs (minesweeper.c:22).

  2. helpers/mine_sweeper_storage.h + helpers/mine_sweeper_storage.c
     Summary: settings persistence and validation.
     Fix/improve:

  - Missing persistence of ensure_solvable_board; only width/height/difficulty/feedback/wrap are stored
    (helpers/mine_sweeper_storage.c:56, helpers/mine_sweeper_storage.c:136).
  - Error-path leaks: flipper_format_file_open_new failure doesn’t free fff_file (helpers/
    mine_sweeper_storage.c:46), and temp_str leaks on early returns (helpers/mine_sweeper_storage.c:98,
    helpers/mine_sweeper_storage.c:100, helpers/mine_sweeper_storage.c:107).
  - Save is non-atomic despite tmp path define; should write temp + rename for crash safety.
  - if(!storage_common_stat(...) == FSE_OK) is precedence-confusing (helpers/mine_sweeper_storage.c:33).

  3. helpers/mine_sweeper_led.h + helpers/mine_sweeper_led.c
     Summary: LED feedback utilities.
     Fix/improve:

  - NotificationSequence references stack-allocated NotificationMessages (helpers/mine_sweeper_led.c:9),
    with delay used as workaround (helpers/mine_sweeper_led.c:27); this is fragile and should be
    converted to static/owned messages.
  - Blocking waits in feedback path can stall input responsiveness (helpers/mine_sweeper_led.c:35,
    helpers/mine_sweeper_led.c:66).

  4. helpers/mine_sweeper_haptic.h + helpers/mine_sweeper_haptic.c
     Summary: vibro feedback patterns.
     Fix/improve:

  - No furi_assert(context) checks before dereference (helpers/mine_sweeper_haptic.c:6).
  - Functions still sleep/reset even when feedback is disabled; return early to avoid useless blocking
    (helpers/mine_sweeper_haptic.c:8, helpers/mine_sweeper_haptic.c:10).
  - Duplication across bump variants; consolidate patterns.

  5. helpers/mine_sweeper_speaker.h + helpers/mine_sweeper_speaker.c
     Summary: sound feedback via notification sequences.
     Fix/improve:

  - Generally clean; main improvements are minor: add context asserts and reduce repeated guard code.
  - Keep this pattern as the template for haptic/LED (non-blocking, sequence-based).

  6. scenes/minesweeper_scene.h + scenes/minesweeper_scene.c
     Summary: scene enum/handler table generation.
     Fix/improve:

  - Core is fine; mostly maintainability checks only (handler count/scene count invariants).

  7. views/start_screen.h + views/start_screen.c
     Summary: custom start view with animation/text/input callback hooks.
     Fix/improve:

  - Solid overall; initialize context defensively in alloc path (views/start_screen.c:129).
  - Minor cleanup only; no major logic bug found.

  8. views/minesweeper_game_screen.h + views/minesweeper_game_screen.c (+ views/
     minesweeper_game_screen_i.h)
     Summary: primary game state, generation, verifier, rendering, input handling, win/lose/reset flow.
     Fix/improve:

  - Potential indefinite generation loop when ensure_solvable is on (views/
    minesweeper_game_screen.c:1689); add retry cap/fallback.
  - OOB feedback calls “happy bump” instead of OOB bump (views/minesweeper_game_screen.c:958).
  - End-screen reset UX requires release then another press, while UI says “PRESS OK” (views/
    minesweeper_game_screen.c:1467, views/minesweeper_game_screen.c:1473).
  - Format specifier mismatch for coordinates (%hhd with int16 fields) (views/
    minesweeper_game_screen.c:1381, views/minesweeper_game_screen.c:1395).
  - Flag action plays flag effect even when no flag was actually placed (e.g., no flags left) (views/
    minesweeper_game_screen.c:1170, views/minesweeper_game_screen.c:1181).
  - Heavy feedback calls inside move/input handlers can cause perceived lag.

  Headerless Scene Files (Important, though not .h+.c pairs)

  - scenes/start_screen_scene.c: any non-back input event advances, including repeats/releases (scenes/
    start_screen_scene.c:14); constrain to intended key/type.
  - scenes/menu_scene.c: center result doesn’t set consumed = true (scenes/menu_scene.c:86); minor
    event-flow cleanup.
  - scenes/settings_scene.c: width/height custom events are swapped (scenes/settings_scene.c:76, scenes/
    settings_scene.c:100).
  - scenes/settings_scene.c: unused enum block (scenes/settings_scene.c:11).
  - scenes/game_screen_scene.c, scenes/confirmation_scene.c, scenes/info_scene.c: mostly fine; low-risk
    cleanup only.

  Suggested Fix Order

  1. Persistence/state correctness: ensure-solvable initialization + read/write + startup usage.
  2. Game loop safety/UX: solvable retry cap, end-screen restart flow, OOB feedback function call.
  3. Storage error handling and atomic write.
  4. Input/render correctness and polish (format specifiers, flag no-op feedback, scene event cleanups).
  5. Refactor feedback modules to remove blocking waits and fragile stack sequence patterns.

