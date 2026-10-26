#pragma once

// Supabase-backed loader authentication.
// Login rule: username + password.
//   username + password -> Supabase Auth (GoTrue) password grant, where the
//                          email is "<username>@loader.local"
//   account_login()     -> binds the HWID and records the login in Postgres
// Network calls run on worker threads; the UI only polls status.

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
// account in the background so a banned or moved account loses its session.
void Tick();

// Starts the login flow on a worker thread. Credentials stay in memory only.
void StartLogin(const std::string& username, const std::string& password);

// Status of the last login attempt.
int LoginStatus();
std::string LoginMessage();

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
