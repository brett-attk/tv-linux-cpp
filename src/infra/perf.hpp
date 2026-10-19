#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace tv {

class MpvPlayer;

namespace perf {

using Clock = std::chrono::steady_clock;

/** True when TV_CPP_PERF=1 (or non-empty truthy value). */
bool enabled();

/** Mark process start (call once from main before GTK run). */
void mark_start();

Clock::time_point start_time();

/** Milliseconds since mark_start(); 0 if not marked. */
int64_t elapsed_ms();

/** Optional auto-play URL from TV_CPP_PERF_CHANNEL_URL. */
std::string channel_url();

/**
 * Emit PERF_READY to stdout and start a 1 Hz PERF_MPV timer while enabled.
 * safe to call once after mpv is attached; no-op if perf mode is off.
 */
void on_player_ready(MpvPlayer& player);

/** Stop the metrics timer (e.g. on shutdown). */
void shutdown();

}  // namespace perf
}  // namespace tv
