// Base preferences file used by performance harnesses
/* globals user_pref */
user_pref("browser.EULA.override", true);
user_pref("browser.addon-watch.interval", -1); // Deactivate add-on watching
// Disable Bookmark backups by default.
user_pref("browser.bookmarks.max_backups", 0);
user_pref("browser.cache.disk.smart_size.enabled", false);
user_pref("browser.chrome.dynamictoolbar", false);
user_pref("browser.contentHandlers.types.0.uri", "http://127.0.0.1/rss?url=%s");
user_pref("browser.contentHandlers.types.1.uri", "http://127.0.0.1/rss?url=%s");
user_pref("browser.contentHandlers.types.2.uri", "http://127.0.0.1/rss?url=%s");
user_pref("browser.contentHandlers.types.3.uri", "http://127.0.0.1/rss?url=%s");
user_pref("browser.contentHandlers.types.4.uri", "http://127.0.0.1/rss?url=%s");
user_pref("browser.contentHandlers.types.5.uri", "http://127.0.0.1/rss?url=%s");
user_pref("browser.link.open_newwindow", 2);
user_pref("browser.newtabpage.activity-stream.default.sites", "");
user_pref("browser.newtabpage.activity-stream.telemetry", false);
user_pref("browser.ping-centre.production.endpoint", "https://127.0.0.1/pingcentre/dummy/");
user_pref("browser.ping-centre.staging.endpoint", "https://127.0.0.1/pingcentre/dummy/");
user_pref("browser.reader.detectedFirstArticle", true);
user_pref("browser.safebrowsing.blockedURIs.enabled", false);
user_pref("browser.safebrowsing.downloads.enabled", false);
user_pref("browser.safebrowsing.downloads.remote.url", "http://127.0.0.1/safebrowsing-dummy/downloads");
user_pref("browser.safebrowsing.malware.enabled", false);
user_pref("browser.safebrowsing.passwords.enabled", false);
user_pref("browser.safebrowsing.phishing.enabled", false);
user_pref("browser.safebrowsing.provider.google.gethashURL", "http://127.0.0.1/safebrowsing-dummy/gethash");
user_pref("browser.safebrowsing.provider.google.updateURL", "http://127.0.0.1/safebrowsing-dummy/update");
user_pref("browser.safebrowsing.provider.google4.gethashURL", "http://127.0.0.1/safebrowsing4-dummy/gethash");
user_pref("browser.safebrowsing.provider.google4.updateURL", "http://127.0.0.1/safebrowsing4-dummy/update");
user_pref("browser.safebrowsing.provider.mozilla.gethashURL", "http://127.0.0.1/safebrowsing-dummy/gethash");
user_pref("browser.safebrowsing.provider.mozilla.updateURL", "http://127.0.0.1/safebrowsing-dummy/update");
user_pref("browser.search.geoip.url", "");
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.tabs.remote.autostart", true);
// Turn off the location bar search suggestions opt-in.  It interferes with
// tests that don't expect it to be there.
user_pref("browser.urlbar.userMadeSearchSuggestionsChoice", true);
user_pref("browser.warnOnQuit", false);
user_pref("datareporting.healthreport.documentServerURI", "http://127.0.0.1/healthreport/");
user_pref("devtools.chrome.enabled", false);
user_pref("devtools.debugger.remote-enabled", false);
user_pref("devtools.theme", "light");
user_pref("devtools.timeline.enabled", false);
user_pref("dom.allow_scripts_to_close_windows", true);
user_pref("dom.disable_open_during_load", false);
user_pref("dom.disable_window_flip", true);
user_pref("dom.disable_window_move_resize", true);
user_pref("extensions.autoDisableScopes", 10);
user_pref("extensions.blocklist.enabled", false);
user_pref("extensions.blocklist.url", "http://127.0.0.1/extensions-dummy/blocklistURL");
user_pref("extensions.checkCompatibility", false);
user_pref("extensions.systemAddon.update.url", "http://127.0.0.1/dummy-system-addons.xml");
user_pref("extensions.update.background.url", "http://127.0.0.1/extensions-dummy/updateBackgroundURL");
user_pref("extensions.update.notifyUser", false);
user_pref("extensions.update.url", "http://127.0.0.1/extensions-dummy/updateURL");
user_pref("identity.fxaccounts.auth.uri", "https://127.0.0.1/fxa-dummy/");
user_pref("identity.fxaccounts.migrateToDevEdition", false);
// Avoid idle-daily notifications, to avoid expensive operations that may
// cause unexpected test timeouts.
user_pref("idle.lastDailyNotification", -1);
user_pref("media.capturestream_hints.enabled", true);
user_pref("media.gmp-manager.url", "http://127.0.0.1/gmpmanager-dummy/update.xml");
// Don't block old libavcodec libraries when testing, because our test systems
// cannot easily be upgraded.
user_pref("media.libavcodec.allow-obsolete", true);
user_pref("media.navigator.enabled", true);
user_pref("media.navigator.permission.disabled", true);
user_pref("media.peerconnection.enabled", true);
// Disable speculative connections so they aren't reported as leaking when they're hanging around.
user_pref("network.http.speculative-parallel-limit", 0);
user_pref("network.proxy.http", "localhost");
user_pref("network.proxy.http_port", 80);
user_pref("network.proxy.type", 1);
// Set places maintenance far in the future (the maximum time possible in an
// int32_t) to avoid it kicking in during tests. The maintenance can take a
// relatively long time which may cause unnecessary intermittents and slow down
// tests. This, like many things, will stop working correctly in 2038.
user_pref("places.database.lastMaintenance", 2147483647);
user_pref("plugin.state.flash", 0);
user_pref("plugins.flashBlock.enabled", false);
user_pref("privacy.reduceTimerPrecision", false); // Bug 1445243 - reduces precision of tests
user_pref("privacy.trackingprotection.annotate_channels", false);
user_pref("privacy.trackingprotection.enabled", false);
user_pref("privacy.trackingprotection.introURL", "http://127.0.0.1/trackingprotection/tour");
user_pref("privacy.trackingprotection.pbmode.enabled", false);
user_pref("security.enable_java", false);
user_pref("security.fileuri.strict_origin_policy", false);
user_pref("toolkit.telemetry.server", "https://127.0.0.1/telemetry-dummy/");
user_pref("startup.homepage_welcome_url", "");
user_pref("startup.homepage_welcome_url.additional", "");
