#include "infra/logging.hpp"
#include "model/channel_store.hpp"
#include "model/hdhomerun.hpp"
#include "model/persist.hpp"
#include "model/settings.hpp"
#include "util/apply_if_open.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>

#include <catch2/catch_test_macros.hpp>

namespace fs = std::filesystem;

namespace {

struct FailSaveStore : tv::ChannelStore {
  using ChannelStore::ChannelStore;
  bool fail = false;
  void save() override {
    if (fail) {
      throw std::system_error(
          std::make_error_code(std::errc::read_only_file_system), "read-only");
    }
    ChannelStore::save();
  }
};

}  // namespace

TEST_CASE("normalize_device_url accepts host and lineup urls", "[hdhr]") {
  REQUIRE(tv::normalize_device_url("192.168.1.27") == "http://192.168.1.27");
  REQUIRE(tv::normalize_device_url("http://192.168.1.27/") == "http://192.168.1.27");
  REQUIRE(tv::normalize_device_url("http://192.168.1.27/lineup.json") ==
          "http://192.168.1.27");
  REQUIRE(tv::normalize_device_url("https://hdhomerun.local/discover.json") ==
          "https://hdhomerun.local");
}

TEST_CASE("normalize_device_url rejects blank", "[hdhr]") {
  REQUIRE_THROWS_AS(tv::normalize_device_url(""), tv::HdhrError);
}

TEST_CASE("validate_fetch_url allows http https", "[hdhr]") {
  REQUIRE(tv::validate_fetch_url("http://192.168.1.27/lineup.json") ==
          "http://192.168.1.27/lineup.json");
  REQUIRE(tv::validate_fetch_url("https://hdhomerun.local/lineup.json") ==
          "https://hdhomerun.local/lineup.json");
}

TEST_CASE("validate_fetch_url rejects non-http schemes", "[hdhr]") {
  for (const char* bad :
       {"file:///etc/passwd", "ftp://192.168.1.27/lineup.json",
        "gopher://example/x", "", "not-a-url"}) {
    REQUIRE_THROWS_AS(tv::validate_fetch_url(bad), tv::HdhrError);
  }
}

TEST_CASE("validate_fetch_url hostname allowlist", "[hdhr]") {
  REQUIRE(tv::validate_fetch_url("http://192.168.1.27:80/lineup.json",
                                 std::string("192.168.1.27")) ==
          "http://192.168.1.27:80/lineup.json");
  REQUIRE_THROWS_AS(
      tv::validate_fetch_url("http://evil.example/lineup.json",
                             std::string("192.168.1.27")),
      tv::HdhrError);
}

TEST_CASE("resolve_lineup_url ignores untrusted lineup urls", "[hdhr]") {
  auto getter = [](const std::string&, double) {
    return nlohmann::json{{"FriendlyName", "HDHR"},
                          {"LineupURL", "file:///tmp/evil.json"}};
  };
  REQUIRE(tv::resolve_lineup_url("http://192.168.1.27", 8.0, getter) ==
          "http://192.168.1.27/lineup.json");
}

TEST_CASE("resolve_lineup_url ignores cross-host lineup url", "[hdhr]") {
  auto getter = [](const std::string&, double) {
    return nlohmann::json{{"LineupURL", "http://attacker.example/lineup.json"}};
  };
  REQUIRE(tv::resolve_lineup_url("http://192.168.1.27", 8.0, getter) ==
          "http://192.168.1.27/lineup.json");
}

TEST_CASE("resolve_lineup_url accepts same-host lineup url", "[hdhr]") {
  auto getter = [](const std::string&, double) {
    return nlohmann::json{{"LineupURL", "http://192.168.1.27/lineup.json"}};
  };
  REQUIRE(tv::resolve_lineup_url("http://192.168.1.27", 8.0, getter) ==
          "http://192.168.1.27/lineup.json");
}

TEST_CASE("parse_lineup skips entries without url and marks drm", "[hdhr]") {
  nlohmann::json payload = nlohmann::json::array(
      {{{"GuideNumber", "2.1"},
        {"GuideName", "NBC"},
        {"URL", "http://192.168.1.27:5004/auto/v2.1"},
        {"Tags", "favorite"}},
       {{"GuideNumber", "4.1"},
        {"GuideName", "FOX"},
        {"URL", "http://192.168.1.27:5004/auto/v4.1"},
        {"Tags", "drm"}},
       {{"GuideNumber", "x"}, {"GuideName", "bad"}, {"URL", ""}}});
  auto channels = tv::parse_lineup(payload);
  REQUIRE(channels.size() == 2);
  REQUIRE(channels[0].display_name() == "2.1  NBC");
  REQUIRE(channels[1].display_name() == "4.1  FOX  (DRM)");
  REQUIRE(channels[1].has_drm());
}

TEST_CASE("parse_lineup scan in progress", "[hdhr]") {
  REQUIRE_THROWS_AS(
      tv::parse_lineup(nlohmann::json{{"ScanInProgress", 1}, {"Channels", nlohmann::json::array()}}),
      tv::HdhrError);
}

TEST_CASE("apply_if_open runs when alive", "[import]") {
  std::vector<std::string> calls;
  auto result = tv::apply_if_open(false, [&](const std::string& value) {
    calls.push_back(value);
    return value;
  }, std::string("ok"));
  REQUIRE(result == "ok");
  REQUIRE(calls == std::vector<std::string>{"ok"});
}

TEST_CASE("apply_if_open skips when closed", "[import]") {
  std::vector<std::string> calls;
  auto result = tv::apply_if_open(true, [&](const std::string& value) {
    calls.push_back(value);
    return value;
  }, std::string("late"));
  REQUIRE_FALSE(result.has_value());
  REQUIRE(calls.empty());
}

TEST_CASE("try_store_write success", "[persist]") {
  auto [value, err] = tv::try_store_write<int>([] { return 42; });
  REQUIRE(value == 42);
  REQUIRE_FALSE(err.has_value());
}

TEST_CASE("try_store_write captures system_error", "[persist]") {
  auto [value, err] = tv::try_store_write<int>([]() -> int {
    throw std::system_error(std::make_error_code(std::errc::no_space_on_device),
                            "disk full");
  });
  REQUIRE_FALSE(value.has_value());
  REQUIRE(err.has_value());
}

TEST_CASE("channel store first start creates empty file", "[store]") {
  const auto tmp = fs::temp_directory_path() / "tv-cpp-test-first";
  fs::remove_all(tmp);
  const auto path = tmp / "tv-cpp" / "channels.json";
  REQUIRE_FALSE(fs::exists(path));
  tv::ChannelStore store(path);
  REQUIRE(fs::exists(path));
  REQUIRE(store.list().empty());
  std::ifstream in(path);
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
  REQUIRE(content.find("\"channels\": []") != std::string::npos);
  fs::remove_all(tmp);
}

TEST_CASE("channel store roundtrip", "[store]") {
  const auto tmp = fs::temp_directory_path() / "tv-cpp-test-roundtrip";
  fs::remove_all(tmp);
  fs::create_directories(tmp);
  const auto path = tmp / "channels.json";
  tv::ChannelStore store(path);
  REQUIRE(store.list().empty());
  auto added = store.add("NBC", "http://192.168.1.27:5004/auto/v2.1");
  tv::ChannelStore reloaded(path);
  auto found = reloaded.get(added.id);
  REQUIRE(found.has_value());
  REQUIRE(found->name == "NBC");
  REQUIRE_FALSE(found->favorite);
  auto neighbor = reloaded.neighbor(added.id, 1);
  REQUIRE(neighbor.has_value());
  REQUIRE(neighbor->id == added.id);
  REQUIRE(reloaded.remove(added.id));
  REQUIRE(reloaded.list().empty());
  fs::remove_all(tmp);
}

TEST_CASE("favorite persists and sorts first", "[store]") {
  const auto tmp = fs::temp_directory_path() / "tv-cpp-test-fav";
  fs::remove_all(tmp);
  fs::create_directories(tmp);
  const auto path = tmp / "channels.json";
  tv::ChannelStore store(path);
  auto a = store.add("A", "http://example/a");
  auto b = store.add("B", "http://example/b");
  auto c = store.add("C", "http://example/c");
  REQUIRE(store.list()[0].name == "A");
  REQUIRE(store.list()[1].name == "B");
  REQUIRE(store.list()[2].name == "C");

  auto updated = store.set_favorite(b.id, true);
  REQUIRE(updated.has_value());
  REQUIRE(updated->favorite);
  REQUIRE(store.list()[0].name == "B");
  REQUIRE(store.list()[1].name == "A");
  REQUIRE(store.list()[2].name == "C");

  REQUIRE(store.neighbor(b.id, 1)->id == a.id);
  REQUIRE(store.neighbor(a.id, 1)->id == c.id);
  REQUIRE(store.neighbor(c.id, 1)->id == b.id);

  tv::ChannelStore reloaded(path);
  REQUIRE(reloaded.get(b.id)->favorite);
  REQUIRE(reloaded.list()[0].name == "B");

  store.set_favorite(b.id, false);
  REQUIRE(store.list()[0].name == "A");
  fs::remove_all(tmp);
}

TEST_CASE("add surfaces error via try_store_write and rolls back", "[store]") {
  const auto tmp = fs::temp_directory_path() / "tv-cpp-test-add-fail";
  fs::remove_all(tmp);
  fs::create_directories(tmp);
  const auto path = tmp / "channels.json";
  FailSaveStore store(path);
  store.fail = true;
  auto [channel, err] = tv::try_store_write<tv::Channel>(
      [&] { return store.add("NBC", "http://192.168.1.27:5004/auto/v2.1"); });
  REQUIRE_FALSE(channel.has_value());
  REQUIRE(err.has_value());
  REQUIRE(store.list().empty());
  fs::remove_all(tmp);
}

TEST_CASE("update rolls back on save failure", "[store]") {
  const auto tmp = fs::temp_directory_path() / "tv-cpp-test-update-fail";
  fs::remove_all(tmp);
  fs::create_directories(tmp);
  const auto path = tmp / "channels.json";
  tv::ChannelStore seed(path);
  auto channel = seed.add("NBC", "http://example/a");

  struct FailUpdateStore : tv::ChannelStore {
    using ChannelStore::ChannelStore;
    explicit FailUpdateStore(fs::path p) : ChannelStore(std::move(p)) {}
    bool fail = false;
    void save() override {
      if (fail) {
        throw std::system_error(
            std::make_error_code(std::errc::read_only_file_system), "read-only");
      }
      ChannelStore::save();
    }
  };

  FailUpdateStore store(path);
  store.fail = true;
  REQUIRE_THROWS_AS(store.update(channel.id, "CBS", "http://example/b"),
                    std::system_error);
  auto found = store.get(channel.id);
  REQUIRE(found.has_value());
  REQUIRE(found->name == "NBC");
  REQUIRE(found->url == "http://example/a");
  fs::remove_all(tmp);
}

TEST_CASE("settings first start and roundtrip", "[settings]") {
  const auto tmp = fs::temp_directory_path() / "tv-cpp-test-settings";
  fs::remove_all(tmp);
  const auto path = tmp / "tv-cpp" / "settings.json";
  REQUIRE_FALSE(fs::exists(path));
  auto settings = tv::AppSettings::load(path);
  REQUIRE(fs::exists(path));
  REQUIRE(settings.osc_enabled);

  tv::AppSettings{false}.save(path);
  REQUIRE_FALSE(tv::AppSettings::load(path).osc_enabled);
  fs::remove_all(tmp);
}
