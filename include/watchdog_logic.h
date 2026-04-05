#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace watchdog {

template <size_t N>
using MutableRingView = std::array<std::string *, N>;

template <size_t N>
using ConstRingView = std::array<const std::string *, N>;

inline std::string format_event_entry(int uptime_seconds, int stage, const std::string &event) {
  char formatted[96];
  std::snprintf(formatted, sizeof(formatted), "u%05d s%d %s", uptime_seconds, stage, event.c_str());
  return std::string(formatted);
}

template <size_t N>
inline void push_ring(const MutableRingView<N> &entries, int &head, int &count, const std::string &entry) {
  static_assert(N > 0, "Ring buffer size must be greater than zero");

  *entries[head] = entry;
  head = (head + 1) % static_cast<int>(N);
  if (count < static_cast<int>(N)) {
    count += 1;
  }
}

template <size_t N>
inline std::string latest_ring(const ConstRingView<N> &entries, int head, int count) {
  static_assert(N > 0, "Ring buffer size must be greater than zero");

  if (count <= 0) {
    return "none";
  }

  int slot = head - 1;
  if (slot < 0) {
    slot += static_cast<int>(N);
  }

  return *entries[slot];
}

template <size_t N>
inline std::string render_ring(const ConstRingView<N> &entries, int head, int count) {
  static_assert(N > 0, "Ring buffer size must be greater than zero");

  if (count <= 0) {
    return "none";
  }

  std::string out;
  for (int offset = 0; offset < count; offset++) {
    int slot = head - 1 - offset;
    if (slot < 0) {
      slot += static_cast<int>(N);
    }

    const std::string &entry = *entries[slot];
    if (entry.empty()) {
      continue;
    }

    if (!out.empty()) {
      out += " | ";
    }
    out += entry;
  }

  return out.empty() ? "none" : out;
}

struct TimeoutTickResult {
  bool enabled;
  int remaining_minutes;
  bool expired;
};

inline TimeoutTickResult tick_timeout(bool enabled, int remaining_minutes) {
  if (!enabled) {
    return {false, 0, false};
  }

  int next_remaining = remaining_minutes > 0 ? remaining_minutes - 1 : 0;
  if (next_remaining <= 0) {
    return {false, 0, true};
  }

  return {true, next_remaining, false};
}

inline int stage_wait_seconds(int stage, int startup_grace_seconds, int max_stage_grace_seconds) {
  if (stage <= 0) {
    return startup_grace_seconds;
  }

  long long wait_seconds = static_cast<long long>(startup_grace_seconds) * (1LL << stage);
  return static_cast<int>(std::min<long long>(wait_seconds, max_stage_grace_seconds));
}

inline int required_powered_on_seconds(
    int stage,
    int startup_grace_seconds,
    int max_stage_grace_seconds,
    int recovery_grace_seconds) {
  return std::max(stage_wait_seconds(stage, startup_grace_seconds, max_stage_grace_seconds), recovery_grace_seconds);
}

inline bool should_clear_self_reboot_guard(int consecutive_successes, int success_threshold, bool self_reboot_attempted) {
  return self_reboot_attempted && consecutive_successes >= success_threshold;
}

inline bool should_reset_auto_cycle_stage(int consecutive_successes, int success_threshold, int auto_cycle_stage) {
  return auto_cycle_stage != 0 && consecutive_successes >= success_threshold;
}

inline bool should_skip_failure(
    bool cycle_in_progress,
    bool cycle_script_running,
    int seconds_since_power_restore,
    int required_powered_on_seconds) {
  return cycle_in_progress || cycle_script_running || seconds_since_power_restore < required_powered_on_seconds;
}

inline bool should_attempt_auto_cycle(
    int failed_requests,
    int failure_threshold,
    bool relay_on,
    bool cycle_script_running) {
  return failed_requests >= failure_threshold && relay_on && !cycle_script_running;
}

inline int next_auto_cycle_stage(int current_stage, int max_stage, bool automatic) {
  if (!automatic) {
    return current_stage;
  }

  return current_stage < max_stage ? current_stage + 1 : current_stage;
}

inline std::string cycle_event_name(bool automatic, const std::string &reason) {
  return std::string(automatic ? "cycle_auto_" : "cycle_manual_") + reason;
}

inline std::string self_reboot_event_name(const std::string &reason) {
  return "self_reboot_preflight_" + reason;
}

inline std::string dry_run_event_name(bool would_self_reboot, const std::string &reason) {
  return std::string(would_self_reboot ? "dry_run_would_self_reboot_" : "dry_run_would_cycle_") + reason;
}

}  // namespace watchdog
