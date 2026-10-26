#pragma once

// Supabase-backed loader authentication.
// Login rule: username + password.
//   The client only calls the server-side Edge Function "loader-auth".
//   The function verifies the password with Supabase Auth, then calls
//   account_login() to bind the HWID, check bans and record the login.
//   At startup a "startup" call sends the client version and the SHA-256 of
//   the running executable; loader_startup() rejects old or modified builds
//   and returns the component list. After the password check a "bootstrap"
//   call repeats those checks with the user token (loader_bootstrap()).
// No password grant or service key is used by the client itself.
// Network calls run on worker threads; the UI only polls status.

#include <string>
#include <vector>

namespace sdauth {

enum Status {
	StatusIdle = 0,
	StatusWorking = 1,
	StatusOk = 2,
	StatusError = 3,
};

// Startup progress (first loading screen, before the login form).
enum StartupStage {
	StartupNone = 0,
	StartupConnecting = 1, // reach the loader-auth function
	StartupIntegrity = 2,  // client version, executable hash, maintenance
	StartupComponents = 3, // component list released by the server
	StartupDone = 4,
};

// Automatic update state when the server rejects this build or reports a newer
// latest_version on a successful check.
enum UpdateState {
	UpdateIdle = 0,
	UpdateDownloading = 1,
	UpdateRestarting = 2,
	UpdateFailed = 3,
};

// Login progress (second loading screen, after the Login button).
// Every stage is decided by the server, not by the client.
enum Stage {
	StageNone = 0,
	StageAuthenticating = 1, // password check against Supabase Auth
	StageAccount = 2,        // ban, HWID binding and rate limit (account_login)
	StageSecurity = 3,       // account, HWID, version and executable hash again
	StageDone = 4,
};

struct Component {
	std::string name;
	std::string version;
	std::string description;
};

struct Session {
	bool valid = false;
	std::string username;
	std::string access_token;
	std::string refresh_token;
	long long expires_at = 0; // unix seconds
};

// Computes the hardware id once and stores it. Safe to call more than once.
void Init();

// Starts the startup check on a worker thread: version, executable hash,
// maintenance flag and the component list. Safe to call again to retry.
void StartStartup();
int StartupStatus();
int StartupStageValue();
std::string StartupMessage();

// Direct executable URL and SHA-256 sent by the server when this build is
// rejected. ReleaseUrl is only a fallback page. Empty when unavailable.
std::string StartupDownloadUrl();
std::string StartupReleaseUrl();
std::string StartupSha256();

// Newest version published on the server. Empty for servers that do not send
// one. Unlike a rejected build, this allows an update before the login form.
std::string StartupLatestVersion();

// True when latest_version is newer than this executable and the server sent a
// download URL with a SHA-256. The startup screen updates automatically.
bool StartupUpdateAvailable();

// Downloads the update, verifies its SHA-256 and restarts through a small
// helper process. Safe to call again after a failure.
void StartUpdate();
int UpdateStatus();
int UpdateProgress();
std::string UpdateMessage();

// Call every frame from the render loop. Refreshes the token and re-checks the
// account in the background so a banned or moved account loses its session.
void Tick();

// Starts the login flow on a worker thread. Credentials stay in memory only.
void StartLogin(const std::string& username, const std::string& password);

// Status of the last login attempt.
int LoginStatus();
std::string LoginMessage();

// Current Stage of the running or last login attempt.
int LoginStage();

// Components the server released after the bootstrap checks passed.
std::vector<Component> Components();

// Reports a client-side validation failure without touching the network.
void ReportLocalError(const std::string& message);

// Clears StatusOk or StatusError so the UI reacts once.
void ResetStatus();

bool HasSession();
Session GetSession();
void SignOut();

// Stable machine fingerprint used for account binding.
const std::string& MachineHwid();

// Usernames are lowercase letters, digits and underscore, 3 to 20 characters.
bool ValidateUsername(const std::string& username, std::string& error);

// Trims whitespace and lowercases a username before validation.
std::string NormalizeUsername(const std::string& username);

} // namespace sdauth
