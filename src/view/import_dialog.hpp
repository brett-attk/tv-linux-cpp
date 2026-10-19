#pragma once

#include "model/hdhomerun.hpp"
#include "util/apply_if_open.hpp"

#include <optional>
#include <set>
#include <string>
#include <vector>

#include <gtkmm.h>

namespace tv {

class HdhrImportDialog : public Gtk::Dialog {
 public:
  HdhrImportDialog(Gtk::Window& parent,
                   const std::vector<std::string>& existing_urls = {});

  std::vector<HdhrChannel> get_selected_channels() const;

 private:
  void on_destroyed();
  void on_fetch_clicked();
  bool idle_fetch_failed(const std::string& message);
  bool idle_fetch_ok(std::vector<HdhrChannel> channels);
  void on_fetch_failed(const std::string& message);
  void on_fetch_ok(const std::vector<HdhrChannel>& channels);
  void clear_list();
  void populate_list();
  void set_all_checks(bool active);
  void update_ok_sensitivity();

  std::set<std::string> existing_urls_;
  std::vector<HdhrChannel> channels_;
  std::vector<Gtk::CheckButton*> checks_;
  bool fetching_ = false;
  bool closed_ = false;

  Gtk::Entry url_entry_;
  Gtk::Button fetch_btn_{"Fetch"};
  Gtk::Label status_label_;
  Gtk::Button select_all_btn_{"Select all"};
  Gtk::Button select_none_btn_{"Select none"};
  Gtk::ListBox list_box_;
};

}  // namespace tv
