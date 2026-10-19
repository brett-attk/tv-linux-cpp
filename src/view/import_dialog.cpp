#include "view/import_dialog.hpp"

#include "infra/logging.hpp"

#include <algorithm>

namespace tv {

HdhrImportDialog::HdhrImportDialog(
    Gtk::Window& parent, const std::vector<std::string>& existing_urls)
    : Gtk::Dialog("Import from HDHomeRun", parent, true),
      status_label_("", Gtk::ALIGN_START) {
  add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  add_button("Add selected", Gtk::RESPONSE_OK);
  set_default_size(480, 520);
  set_default_response(Gtk::RESPONSE_OK);

  for (const auto& url : existing_urls) {
    if (!url.empty()) {
      existing_urls_.insert(url);
    }
  }
  signal_hide().connect(sigc::mem_fun(*this, &HdhrImportDialog::on_destroyed));

  auto* content = get_content_area();
  content->set_spacing(8);
  content->set_margin_top(12);
  content->set_margin_bottom(12);
  content->set_margin_start(12);
  content->set_margin_end(12);

  auto* url_row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
  auto* url_label = Gtk::manage(new Gtk::Label("Device URL:", Gtk::ALIGN_START));
  url_entry_.set_placeholder_text("http://192.168.1.27 or hdhomerun.local");
  url_entry_.set_hexpand(true);
  url_entry_.set_activates_default(false);
  url_entry_.signal_activate().connect(
      sigc::mem_fun(*this, &HdhrImportDialog::on_fetch_clicked));
  fetch_btn_.signal_clicked().connect(
      sigc::mem_fun(*this, &HdhrImportDialog::on_fetch_clicked));
  url_row->pack_start(*url_label, Gtk::PACK_SHRINK);
  url_row->pack_start(url_entry_, Gtk::PACK_EXPAND_WIDGET);
  url_row->pack_start(fetch_btn_, Gtk::PACK_SHRINK);
  content->pack_start(*url_row, Gtk::PACK_SHRINK);

  status_label_.set_line_wrap(true);
  status_label_.set_text("Enter the HDHomeRun address and click Fetch.");
  content->pack_start(status_label_, Gtk::PACK_SHRINK);

  auto* select_row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
  select_all_btn_.signal_clicked().connect([this] { set_all_checks(true); });
  select_none_btn_.signal_clicked().connect([this] { set_all_checks(false); });
  select_all_btn_.set_sensitive(false);
  select_none_btn_.set_sensitive(false);
  select_row->pack_start(select_all_btn_, Gtk::PACK_SHRINK);
  select_row->pack_start(select_none_btn_, Gtk::PACK_SHRINK);
  content->pack_start(*select_row, Gtk::PACK_SHRINK);

  auto* scrolled = Gtk::manage(new Gtk::ScrolledWindow());
  scrolled->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
  scrolled->set_shadow_type(Gtk::SHADOW_IN);
  scrolled->set_vexpand(true);
  scrolled->set_min_content_height(300);

  list_box_.set_selection_mode(Gtk::SELECTION_NONE);
  scrolled->add(list_box_);
  content->pack_start(*scrolled, Gtk::PACK_EXPAND_WIDGET);

  show_all();
  update_ok_sensitivity();
}

HdhrImportDialog::~HdhrImportDialog() {
  if (alive_) {
    alive_->store(false);
  }
  join_fetch_thread();
}

void HdhrImportDialog::join_fetch_thread() {
  if (fetch_thread_.joinable()) {
    fetch_thread_.join();
  }
}

void HdhrImportDialog::on_destroyed() {
  if (alive_) {
    alive_->store(false);
  }
}

void HdhrImportDialog::on_fetch_clicked() {
  if (fetching_ || !alive_ || !alive_->load()) {
    return;
  }
  const std::string url = url_entry_.get_text();
  if (url.empty()) {
    status_label_.set_text("Enter an HDHomeRun device URL or IP address.");
    return;
  }

  join_fetch_thread();

  fetching_ = true;
  fetch_btn_.set_sensitive(false);
  url_entry_.set_sensitive(false);
  status_label_.set_text("Fetching lineup…");
  clear_list();

  auto alive = alive_;
  fetch_thread_ = std::thread([this, alive, url] {
    try {
      auto channels = fetch_lineup(url);
      Glib::signal_idle().connect_once(
          [this, alive, channels = std::move(channels)]() mutable {
            if (!alive || !alive->load()) {
              return;
            }
            on_fetch_ok(channels);
          });
    } catch (const HdhrError& exc) {
      log_error(std::string("HDHomeRun import failed: ") + exc.what());
      const std::string message = exc.what();
      Glib::signal_idle().connect_once([this, alive, message] {
        if (!alive || !alive->load()) {
          return;
        }
        on_fetch_failed(message);
      });
    } catch (const std::exception& exc) {
      log_exception(std::string("Unexpected HDHomeRun import failure: ") +
                    exc.what());
      const std::string message =
          std::string("Unexpected error: ") + exc.what() + ". See " +
          log_path().string();
      Glib::signal_idle().connect_once([this, alive, message] {
        if (!alive || !alive->load()) {
          return;
        }
        on_fetch_failed(message);
      });
    }
  });
}

void HdhrImportDialog::on_fetch_failed(const std::string& message) {
  fetching_ = false;
  fetch_btn_.set_sensitive(true);
  url_entry_.set_sensitive(true);
  status_label_.set_text(message + "\nDetails: " + log_path().string());
  select_all_btn_.set_sensitive(false);
  select_none_btn_.set_sensitive(false);
  update_ok_sensitivity();
}

void HdhrImportDialog::on_fetch_ok(const std::vector<HdhrChannel>& channels) {
  fetching_ = false;
  fetch_btn_.set_sensitive(true);
  url_entry_.set_sensitive(true);
  channels_ = channels;
  populate_list();
  const int already = static_cast<int>(std::count_if(
      channels_.begin(), channels_.end(),
      [this](const HdhrChannel& c) { return existing_urls_.count(c.url) > 0; }));
  std::string status =
      "Found " + std::to_string(channels_.size()) + " channel(s)";
  if (already) {
    status += " (" + std::to_string(already) + " already in your list).";
  } else {
    status += ".";
  }
  status_label_.set_text(status);
  select_all_btn_.set_sensitive(true);
  select_none_btn_.set_sensitive(true);
  update_ok_sensitivity();
}

void HdhrImportDialog::clear_list() {
  channels_.clear();
  checks_.clear();
  for (auto* child : list_box_.get_children()) {
    list_box_.remove(*child);
  }
}

void HdhrImportDialog::populate_list() {
  checks_.clear();
  for (auto* child : list_box_.get_children()) {
    list_box_.remove(*child);
  }

  for (const auto& channel : channels_) {
    auto* row = Gtk::manage(new Gtk::ListBoxRow());
    row->set_activatable(false);
    auto* box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));
    box->set_margin_start(8);
    box->set_margin_end(8);
    box->set_margin_top(4);
    box->set_margin_bottom(4);

    auto* check = Gtk::manage(new Gtk::CheckButton());
    const bool already = existing_urls_.count(channel.url) > 0;
    if (already) {
      check->set_active(true);
      check->set_sensitive(false);
      check->set_tooltip_text("Already in your channel list");
    } else {
      check->set_active(false);
      check->signal_toggled().connect([this] { update_ok_sensitivity(); });
    }

    auto* label =
        Gtk::manage(new Gtk::Label(channel.display_name(), Gtk::ALIGN_START));
    label->set_ellipsize(Pango::ELLIPSIZE_END);
    label->set_hexpand(true);
    if (already) {
      label->set_text(channel.display_name() + "  — already added");
    }

    box->pack_start(*check, Gtk::PACK_SHRINK);
    box->pack_start(*label, Gtk::PACK_EXPAND_WIDGET);
    row->add(*box);
    list_box_.append(*row);
    checks_.push_back(check);
  }
  list_box_.show_all();
}

void HdhrImportDialog::set_all_checks(bool active) {
  for (auto* check : checks_) {
    if (check->get_sensitive()) {
      check->set_active(active);
    }
  }
  update_ok_sensitivity();
}

void HdhrImportDialog::update_ok_sensitivity() {
  auto* ok = get_widget_for_response(Gtk::RESPONSE_OK);
  if (ok) {
    ok->set_sensitive(!get_selected_channels().empty());
  }
}

std::vector<HdhrChannel> HdhrImportDialog::get_selected_channels() const {
  std::vector<HdhrChannel> selected;
  for (size_t i = 0; i < channels_.size() && i < checks_.size(); ++i) {
    if (existing_urls_.count(channels_[i].url)) {
      continue;
    }
    if (checks_[i]->get_active()) {
      selected.push_back(channels_[i]);
    }
  }
  return selected;
}

}  // namespace tv
