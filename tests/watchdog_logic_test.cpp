#include <cstdlib>
#include <iostream>
#include <string>

#include "include/watchdog_logic.h"

namespace {

void expect(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void test_ring_helpers() {
  std::string a, b, c;
  const std::array<std::string *, 3> ring = {&a, &b, &c};
  int head = 0;
  int count = 0;

  watchdog_logic::push_ring(ring, head, count, "first");
  watchdog_logic::push_ring(ring, head, count, "second");
  expect(count == 2, "ring count increments");
  expect(watchdog_logic::latest_ring<3>({&a, &b, &c}, head, count) == "second", "latest ring item tracks head");
  expect(watchdog_logic::render_ring<3>({&a, &b, &c}, head, count) == "second | first", "render ring orders newest first");

  watchdog_logic::push_ring(ring, head, count, "third");
  watchdog_logic::push_ring(ring, head, count, "fourth");
  expect(count == 3, "ring count caps at capacity");
  expect(watchdog_logic::render_ring<3>({&a, &b, &c}, head, count) == "fourth | third | second", "ring overwrites oldest entry");
}

void test_timeout_tick() {
  auto disabled = watchdog_logic::tick_timeout(false, 10);
  expect(!disabled.enabled && disabled.remaining_minutes == 0 && !disabled.expired, "disabled timeout stays cleared");

  auto running = watchdog_logic::tick_timeout(true, 3);
  expect(running.enabled && running.remaining_minutes == 2 && !running.expired, "running timeout decrements");

  auto expired = watchdog_logic::tick_timeout(true, 1);
  expect(!expired.enabled && expired.remaining_minutes == 0 && expired.expired, "timeout expires at zero");
}

void test_stage_math() {
  expect(watchdog_logic::stage_wait_seconds(0, 600, 14400) == 600, "stage zero uses startup grace");
  expect(watchdog_logic::stage_wait_seconds(1, 600, 14400) == 1200, "stage one doubles grace");
  expect(watchdog_logic::stage_wait_seconds(5, 600, 14400) == 14400, "stage wait caps at max");
  expect(watchdog_logic::required_powered_on_seconds(0, 600, 14400, 900) == 900, "recovery grace can dominate required time");
  expect(watchdog_logic::required_powered_on_seconds(2, 600, 14400, 600) == 2400, "stage grace can dominate required time");
}

void test_success_and_failure_guards() {
  expect(watchdog_logic::should_clear_self_reboot_guard(3, 3, true), "self-reboot guard clears at threshold");
  expect(!watchdog_logic::should_clear_self_reboot_guard(2, 3, true), "self-reboot guard does not clear early");
  expect(watchdog_logic::should_reset_auto_cycle_stage(3, 3, 2), "auto cycle stage resets at threshold");
  expect(!watchdog_logic::should_reset_auto_cycle_stage(3, 3, 0), "stage zero does not reset");

  expect(watchdog_logic::should_skip_failure(true, false, 1000, 600), "active cycle skips failure counting");
  expect(watchdog_logic::should_skip_failure(false, true, 1000, 600), "running cycle script skips failure counting");
  expect(watchdog_logic::should_skip_failure(false, false, 599, 600), "grace window skips failure counting");
  expect(!watchdog_logic::should_skip_failure(false, false, 600, 600), "failures count after grace window");

  expect(watchdog_logic::should_attempt_auto_cycle(4, 4, true, false), "auto cycle triggers at threshold");
  expect(!watchdog_logic::should_attempt_auto_cycle(3, 4, true, false), "auto cycle waits for threshold");
  expect(!watchdog_logic::should_attempt_auto_cycle(4, 4, false, false), "auto cycle requires relay on");
  expect(!watchdog_logic::should_attempt_auto_cycle(4, 4, true, true), "auto cycle waits for script idle");
}

void test_stage_and_event_names() {
  expect(watchdog_logic::next_auto_cycle_stage(0, 5, true) == 1, "automatic cycles advance stage");
  expect(watchdog_logic::next_auto_cycle_stage(5, 5, true) == 5, "automatic stage caps");
  expect(watchdog_logic::next_auto_cycle_stage(2, 5, false) == 2, "manual cycle does not advance stage");

  expect(watchdog_logic::cycle_event_name(true, "watchdog") == "cycle_auto_watchdog", "automatic event name is stable");
  expect(watchdog_logic::cycle_event_name(false, "api") == "cycle_manual_api", "manual event name is stable");
  expect(watchdog_logic::self_reboot_event_name("watchdog") == "self_reboot_preflight_watchdog", "self reboot event name is stable");
  expect(watchdog_logic::dry_run_event_name(true, "watchdog") == "dry_run_would_self_reboot_watchdog", "dry-run self reboot event name is stable");
  expect(watchdog_logic::dry_run_event_name(false, "watchdog") == "dry_run_would_cycle_watchdog", "dry-run cycle event name is stable");
  expect(watchdog_logic::format_event_entry(123, 2, "cycle_auto_watchdog") == "u00123 s2 cycle_auto_watchdog", "event formatting is stable");
}

void test_reset_reason_names() {
  expect(watchdog_logic::reset_reason_name(1) == "poweron", "power-on reset is named");
  expect(watchdog_logic::reset_reason_name(3) == "sw", "deliberate self-reboot is named");
  expect(watchdog_logic::reset_reason_name(4) == "panic", "panic reset is named");
  expect(watchdog_logic::reset_reason_name(6) == "task_wdt", "blocked main loop reset is named");
  expect(watchdog_logic::reset_reason_name(9) == "brownout", "brownout reset is named");
  expect(watchdog_logic::reset_reason_name(0) == "unknown", "unknown reset is named");
  expect(watchdog_logic::reset_reason_name(42) == "reset_42", "unrecognised reset keeps its number");

  // Boot journal entries are built from these names, so they must stay short.
  for (int reason = 0; reason <= 16; reason++) {
    expect(watchdog_logic::reset_reason_name(reason).size() <= 12, "reset names stay short");
  }
  expect(
    watchdog_logic::format_event_entry(300, 1, "boot_" + watchdog_logic::reset_reason_name(6)) ==
      "u00300 s1 boot_task_wdt",
    "boot journal entry carries the reset reason");
}

}  // namespace

int main() {
  test_ring_helpers();
  test_timeout_tick();
  test_stage_math();
  test_success_and_failure_guards();
  test_stage_and_event_names();
  test_reset_reason_names();
  std::cout << "watchdog_logic_test: OK\n";
  return 0;
}
