#pragma once

// Supabase-backed loader authentication.
// Login rule: username + password only.
//   username + password -> Supabase Auth (GoTrue) password grant
//   hardware id         -> public.account_login() RPC, bound on first login
// Network calls run on worker threads, the UI only polls status.

#include <string>

namespace sdauth {

enum Status {
	StatusIdle = 0,
	StatusWorking = 1,
	StatusOk = 2,
	StatusError = 3,
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

// Call every frame from the render loop. Refreshes the token and re-checks the
// account in the background so a banned account or a different machine locks
// the loader.
void Tick();

// Starts the login flow on a worker thread.
void StartLogin(const std::string& username, const std::string& password);

// Status of the last login attempt.
int LoginStatus();
std::string LoginMessage();

// Reports a client-side validation failure without touching the network.
void ReportLocalError(const std::string& message);

// Clears StatusOk so the UI reacts to a login once.
void ResetStatus();

bool HasSession();
Session GetSession();
void SignOut();

// Saved login, encrypted with Windows DPAPI for the current Windows user.
struct SavedLogin {
	std::string username;
	std::string password;
};

bool LoadSavedLogin(SavedLogin& saved);
bool SaveLogin(const std::string& username, const std::string& password);
void ForgetSavedLogin();

// Stable machine fingerprint used for account binding.
const std::string& MachineHwid();

// Usernames are lowercase letters, digits and underscore, 3 to 20 characters.
bool ValidateUsername(const std::string& username, std::string& error);

} // namespace sdauth
