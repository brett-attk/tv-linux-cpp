#include "infra/mpv_player.hpp"

#include "infra/logging.hpp"

#include <algorithm>
#include <chrono>
#include <clocale>
#include <cstring>
#include <stdexcept>

#include <mpv/client.h>

namespace tv {
namespace {

void set_option_string(mpv_handle* mpv, const char* name, const char* value) {
  mpv_set_option_string(mpv, name, value);
}

void set_property_string(mpv_handle* mpv, const char* name, const char* value) {
  mpv_set_property_string(mpv, name, value);
}

}  // namespace

MpvPlayer::MpvPlayer(bool osc_enabled) : osc_enabled_(osc_enabled) {
  worker_ = std::thread([this] { run_worker(); });
}

MpvPlayer::~MpvPlayer() { shutdown(); }

void MpvPlayer::run_worker() {
  while (true) {
    std::optional<Job> job;
    {
      std::unique_lock<std::mutex> lock(jobs_mutex_);
      jobs_cv_.wait(lock, [&] { return !jobs_.empty(); });
      job = std::move(jobs_.front());
      jobs_.pop();
    }
    if (!job) {
      break;
    }
    try {
      (*job)();
    } catch (const std::exception& exc) {
      log_exception(std::string("mpv worker job failed: ") + exc.what());
    } catch (...) {
      log_exception("mpv worker job failed");
    }
  }
}

void MpvPlayer::submit(Job job) {
  {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    jobs_.push(std::move(job));
  }
  jobs_cv_.notify_one();
}

void MpvPlayer::attach(int64_t wid) {
  if (player_ != nullptr) {
    return;
  }
  // GTK may have changed the process locale; libmpv requires C numerics.
  setlocale(LC_NUMERIC, "C");
  mpv_handle* mpv = mpv_create();
  if (!mpv) {
    throw std::runtime_error("mpv_create failed");
  }

  const std::string wid_str = std::to_string(wid);
  set_option_string(mpv, "wid", wid_str.c_str());
  set_option_string(mpv, "vo", "x11");
  set_option_string(mpv, "force-window", "no");
  set_option_string(mpv, "idle", "yes");
  set_option_string(mpv, "input-default-bindings", "yes");
  set_option_string(mpv, "input-vo-keyboard", "yes");
  set_option_string(mpv, "osc", osc_enabled_ ? "yes" : "no");
  set_option_string(mpv, "keepaspect", "yes");
  set_option_string(mpv, "keepaspect-window", "no");
  set_option_string(mpv, "cache", "yes");
  set_option_string(mpv, "demuxer-max-bytes", "8MiB");
  set_option_string(mpv, "demuxer-readahead-secs", "1.5");
  set_option_string(mpv, "volume", std::to_string(volume_).c_str());

  if (mpv_initialize(mpv) < 0) {
    mpv_terminate_destroy(mpv);
    throw std::runtime_error("mpv_initialize failed");
  }

  mpv_observe_property(mpv, 0, "fullscreen", MPV_FORMAT_FLAG);
  player_ = mpv;
  events_stop_ = false;
  event_thread_ = std::thread([this, mpv] { run_events(mpv); });
  submit([this] { apply_audio_state(); });
  submit([this] { apply_osc(); });
}

void MpvPlayer::run_events(mpv_handle* mpv) {
  while (!events_stop_) {
    mpv_event* event = mpv_wait_event(mpv, 0.2);
    if (!event) {
      continue;
    }
    if (event->event_id == MPV_EVENT_SHUTDOWN) {
      break;
    }
    if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
      auto* prop = static_cast<mpv_event_property*>(event->data);
      if (prop && prop->name && std::strcmp(prop->name, "fullscreen") == 0 &&
          prop->format == MPV_FORMAT_FLAG && prop->data) {
        const bool enabled = *static_cast<int*>(prop->data) != 0;
        fullscreen_ = enabled;
        if (on_fullscreen_) {
          on_fullscreen_(enabled);
        }
      }
    }
  }
}

void MpvPlayer::set_fullscreen_handler(FullscreenHandler handler) {
  on_fullscreen_ = std::move(handler);
}

void MpvPlayer::play(const std::string& url) {
  int gen = 0;
  {
    std::lock_guard<std::mutex> lock(lock_);
    media_url_ = url;
    ++media_gen_;
    gen = media_gen_;
  }
  submit([this, gen] { apply_media(gen); });
}

void MpvPlayer::stop() {
  int gen = 0;
  {
    std::lock_guard<std::mutex> lock(lock_);
    media_url_.reset();
    ++media_gen_;
    gen = media_gen_;
  }
  submit([this, gen] { apply_media(gen); });
}

void MpvPlayer::apply_media(int gen) {
  std::optional<std::string> url;
  mpv_handle* player = nullptr;
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (gen != media_gen_) {
      return;
    }
    url = media_url_;
    player = player_;
  }
  if (!player) {
    return;
  }
  if (url) {
    const char* args[] = {"loadfile", url->c_str(), "replace", nullptr};
    if (mpv_command_async(player, 0, args) < 0) {
      log_exception("mpv async loadfile failed; trying sync fallback");
      mpv_command(player, args);
    }
  } else {
    const char* args[] = {"stop", nullptr};
    if (mpv_command_async(player, 0, args) < 0) {
      log_exception("mpv async stop failed; trying sync fallback");
      mpv_command(player, args);
    }
  }
}

void MpvPlayer::set_volume(int volume) {
  volume_ = std::clamp(volume, 0, 100);
  submit([this] { apply_audio_state(); });
}

int MpvPlayer::volume_up(int step) {
  set_volume(volume_ + step);
  return volume_;
}

int MpvPlayer::volume_down(int step) {
  set_volume(volume_ - step);
  return volume_;
}

bool MpvPlayer::set_mute(bool muted) {
  muted_ = muted;
  submit([this] { apply_audio_state(); });
  return muted_;
}

bool MpvPlayer::mute_toggle() { return set_mute(!muted_); }

void MpvPlayer::apply_audio_state() {
  mpv_handle* player = player_;
  if (!player) {
    return;
  }
  double vol = volume_;
  mpv_set_property(player, "volume", MPV_FORMAT_DOUBLE, &vol);
  int mute = muted_ ? 1 : 0;
  mpv_set_property(player, "mute", MPV_FORMAT_FLAG, &mute);
}

bool MpvPlayer::set_osc_enabled(bool enabled) {
  osc_enabled_ = enabled;
  submit([this] { apply_osc(); });
  return osc_enabled_;
}

void MpvPlayer::apply_osc() {
  mpv_handle* player = player_;
  if (!player) {
    return;
  }
  set_property_string(player, "osc", osc_enabled_ ? "yes" : "no");
  if (osc_enabled_) {
    const char* args[] = {"script-message", "osc-visibility", "auto", "no-osd",
                          nullptr};
    mpv_command(player, args);
  } else {
    const char* args[] = {"script-message", "osc-visibility", "never", "no-osd",
                          nullptr};
    mpv_command(player, args);
  }
}

void MpvPlayer::set_fullscreen(bool enabled) {
  fullscreen_ = enabled;
  submit([this, enabled] { apply_fullscreen(enabled); });
}

void MpvPlayer::apply_fullscreen(bool enabled) {
  mpv_handle* player = player_;
  if (!player) {
    return;
  }
  int flag = enabled ? 1 : 0;
  mpv_set_property(player, "fullscreen", MPV_FORMAT_FLAG, &flag);
}

bool MpvPlayer::toggle_fullscreen() {
  set_fullscreen(!fullscreen_);
  return fullscreen_;
}

void MpvPlayer::shutdown() {
  if (stopping_.exchange(true)) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(lock_);
    media_url_.reset();
    ++media_gen_;
  }

  std::mutex done_mutex;
  std::condition_variable done_cv;
  bool done = false;

  events_stop_ = true;
  submit([this, &done_mutex, &done_cv, &done] {
    mpv_handle* player = player_;
    player_ = nullptr;
    if (player) {
      const char* args[] = {"quit", nullptr};
      mpv_command(player, args);
    }
    // Let the event thread observe shutdown / events_stop_ before destroy.
    {
      std::lock_guard<std::mutex> lock(done_mutex);
      done = true;
    }
    done_cv.notify_one();
    if (player) {
      // Brief pause so run_events can exit wait.
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      mpv_terminate_destroy(player);
    }
  });
  {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    jobs_.push(std::nullopt);
  }
  jobs_cv_.notify_one();

  {
    std::unique_lock<std::mutex> lock(done_mutex);
    done_cv.wait_for(lock, std::chrono::milliseconds(1500), [&] { return done; });
  }
  if (event_thread_.joinable()) {
    event_thread_.join();
  }
  if (worker_.joinable()) {
    worker_.join();
  }
}

}  // namespace tv
