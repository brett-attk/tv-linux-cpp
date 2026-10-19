#pragma once

#include "model/hdhomerun.hpp"

#include <atomic>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <gtkmm.h>

namespace tv {

class HdhrImportDialog : public Gtk::Dialog {
 public:
  explicit HdhrImportDialog(Gtk::Window& parent,
                            const std::vector<std::string>& existing_urls = {});
  ~HdhrImportDialog() override;

  std::vector<HdhrChannel> get_selected_channels() const;

 private:
  void on_destroyed();
  void on_fetch_clicked();
  void on_fetch_failed(const std::string& message);
  void on_fetch_ok(const std::vector<HdhrChannel>& channels);
  void clear_list();
  void populate_list();
  void set_all_checks(bool active);
  void update_ok_sensitivity();
  void join_fetch_thread();

  std::set<std::string> existing_urls_;
  std::vector<HdhrChannel> channels_;
  std::vector<Gtk::CheckButton*> checks_;
  bool fetching_ = false;
  std::shared_ptr<std::atomic<bool>> alive_ =
      std::make_shared<std::atomic<bool>>(true);
  std::thread fetch_thread_;

  Gtk::Entry url_entry_;
  Gtk::Button fetch_btn_{"Fetch"};
  Gtk::Label status_label_;
  Gtk::Button select_all_btn_{"Select all"};
  Gtk::Button select_none_btn_{"Select none"};
  Gtk::ListBox list_box_;
};

}  // namespace tv
