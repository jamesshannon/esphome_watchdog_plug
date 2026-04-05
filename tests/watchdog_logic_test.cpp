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

  watchdog::push_ring(ring, head, count, "first");
  watchdog::push_ring(ring, head, count, "second");
  expect(count == 2, "ring count increments");
  expect(watchdog::latest_ring<3>({&a, &b, &c}, head, count) == "second", "latest ring item tracks head");
  expect(watchdog::render_ring<3>({&a, &b, &c}, head, count) == "second | first", "render ring orders newest first");

  watchdog::push_ring(ring, head, count, "third");
  watchdog::push_ring(ring, head, count, "fourth");
  expect(count == 3, "ring count caps at capacity");
  expect(watchdog::render_ring<3>({&a, &b, &c}, head, count) == "fourth | third | second", "ring overwrites oldest entry");
}

void test_timeout_tick() {
  auto disabled = watchdog::tick_timeout(false, 10);
  expect(!disabled.enabled && disabled.remaining_minutes == 0 && !disabled.expired, "disabled timeout stays cleared");

  auto running = watchdog::tick_timeout(true, 3);
  expect(running.enabled && running.remaining_minutes == 2 && !running.expired, "running timeout decrements");

  auto expired = watchdog::tick_timeout(true, 1);
  expect(!expired.enabled && expired.remaining_minutes == 0 && expired.expired, "timeout expires at zero");
}

void test_stage_math() {
  expect(watchdog::stage_wait_seconds(0, 600, 14400) == 600, "stage zero uses startup grace");
  expect(watchdog::stage_wait_seconds(1, 600, 14400) == 1200, "stage one doubles grace");
  expect(watchdog::stage_wait_seconds(5, 600, 14400) == 14400, "stage wait caps at max");
  expect(watchdog::required_powered_on_seconds(0, 600, 14400, 900) == 900, "recovery grace can dominate required time");
  expect(watchdog::required_powered_on_seconds(2, 600, 14400, 600) == 2400, "stage grace can dominate required time");
}

void test_success_and_failure_guards() {
  expect(watchdog::should_clear_self_reboot_guard(3, 3, true), "self-reboot guard clears at threshold");
  expect(!watchdog::should_clear_self_reboot_guard(2, 3, true), "self-reboot guard does not clear early");
  expect(watchdog::should_reset_auto_cycle_stage(3, 3, 2), "auto cycle stage resets at threshold");
  expect(!watchdog::should_reset_auto_cycle_stage(3, 3, 0), "stage zero does not reset");

  expect(watchdog::should_skip_failure(true, false, 1000, 600), "active cycle skips failure counting");
  expect(watchdog::should_skip_failure(false, true, 1000, 600), "running cycle script skips failure counting");
  expect(watchdog::should_skip_failure(false, false, 599, 600), "grace window skips failure counting");
  expect(!watchdog::should_skip_failure(false, false, 600, 600), "failures count after grace window");

  expect(watchdog::should_attempt_auto_cycle(4, 4, true, false), "auto cycle triggers at threshold");
  expect(!watchdog::should_attempt_auto_cycle(3, 4, true, false), "auto cycle waits for threshold");
  expect(!watchdog::should_attempt_auto_cycle(4, 4, false, false), "auto cycle requires relay on");
  expect(!watchdog::should_attempt_auto_cycle(4, 4, true, true), "auto cycle waits for script idle");
}

void test_stage_and_event_names() {
  expect(watchdog::next_auto_cycle_stage(0, 5, true) == 1, "automatic cycles advance stage");
  expect(watchdog::next_auto_cycle_stage(5, 5, true) == 5, "automatic stage caps");
  expect(watchdog::next_auto_cycle_stage(2, 5, false) == 2, "manual cycle does not advance stage");

  expect(watchdog::cycle_event_name(true, "watchdog") == "cycle_auto_watchdog", "automatic event name is stable");
  expect(watchdog::cycle_event_name(false, "api") == "cycle_manual_api", "manual event name is stable");
  expect(watchdog::self_reboot_event_name("watchdog") == "self_reboot_preflight_watchdog", "self reboot event name is stable");
  expect(watchdog::dry_run_event_name(true, "watchdog") == "dry_run_would_self_reboot_watchdog", "dry-run self reboot event name is stable");
  expect(watchdog::dry_run_event_name(false, "watchdog") == "dry_run_would_cycle_watchdog", "dry-run cycle event name is stable");
  expect(watchdog::format_event_entry(123, 2, "cycle_auto_watchdog") == "u00123 s2 cycle_auto_watchdog", "event formatting is stable");
}

}  // namespace

int main() {
  test_ring_helpers();
  test_timeout_tick();
  test_stage_math();
  test_success_and_failure_guards();
  test_stage_and_event_names();
  std::cout << "watchdog_logic_test: OK\n";
  return 0;
}
