#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>

struct mpv_handle;

namespace tv {

class MpvPlayer {
 public:
  using FullscreenHandler = std::function<void(bool)>;

  explicit MpvPlayer(bool osc_enabled = true);
  ~MpvPlayer();

  MpvPlayer(const MpvPlayer&) = delete;
  MpvPlayer& operator=(const MpvPlayer&) = delete;

  bool ready() const {
    std::lock_guard<std::mutex> lock(lock_);
    return player_ != nullptr;
  }

  void attach(int64_t wid);
  void play(const std::string& url);
  void stop();

  void set_volume(int volume);
  int volume_up(int step = 5);
  int volume_down(int step = 5);
  bool set_mute(bool muted);
  bool mute_toggle();

  bool set_osc_enabled(bool enabled);
  bool osc_enabled() const { return osc_enabled_; }
  int volume() const { return volume_; }
  bool muted() const { return muted_; }

  void set_fullscreen(bool enabled);
  bool toggle_fullscreen();
  void set_fullscreen_handler(FullscreenHandler handler);

  /** Best-effort sync read of mpv estimated-vf-fps (0 if unavailable). */
  double estimated_fps();
  /** Best-effort sync read of mpv frame-drop-count (0 if unavailable). */
  int64_t frame_drop_count();

  void shutdown();

 private:
  using Job = std::function<void()>;

  void run_worker();
  void submit(Job job);
  void apply_media(int gen);
  void apply_audio_state();
  void apply_osc();
  void apply_fullscreen(bool enabled);

  void run_events(mpv_handle* mpv);

  mpv_handle* player_ = nullptr;
  FullscreenHandler on_fullscreen_;
  int volume_ = 80;
  bool muted_ = false;
  bool fullscreen_ = false;
  bool osc_enabled_ = true;

  mutable std::mutex lock_;
  std::optional<std::string> media_url_;
  int media_gen_ = 0;

  std::mutex jobs_mutex_;
  std::condition_variable jobs_cv_;
  std::queue<std::optional<Job>> jobs_;
  std::thread worker_;
  std::thread event_thread_;
  std::atomic<bool> stopping_{false};
  std::atomic<bool> events_stop_{false};
};

}  // namespace tv
