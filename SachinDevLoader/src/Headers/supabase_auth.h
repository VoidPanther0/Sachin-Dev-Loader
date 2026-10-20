#pragma once

// Supabase-backed loader authentication.
// Login rule: username + password + license key. All three must match.
//   username + password -> Supabase Auth (GoTrue) password grant
//   license key         -> public.claim_license() RPC, checked in Postgres
// Network calls run on worker threads, the UI only polls status.

#include <string>

namespace sdauth {

enum Status {
	StatusIdle = 0,
	StatusWorking = 1,
	StatusOk = 2,
	StatusError = 3,
};

struct LicenseInfo {
	std::string label;
	std::string key_prefix;
	std::string expires_at;
	int days_left = 0;
	std::string hwid;
};

struct Session {
	bool valid = false;
	std::string username;
	std::string access_token;
	std::string refresh_token;
	long long expires_at = 0; // unix seconds
	LicenseInfo license;
};

// Computes the hardware id once and stores it. Safe to call more than once.
void Init();

// Call every frame from the render loop. Refreshes the token and re-checks the
// license in the background so a revoked or expired key locks the loader.
void Tick();

// Starts the login flow on a worker thread. Key material stays in memory only.
void StartLogin(const std::string& username, const std::string& password, const std::string& license_key);

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

// Stable machine fingerprint used for license binding.
const std::string& MachineHwid();

// Usernames are lowercase letters, digits and underscore, 3 to 20 characters.
bool ValidateUsername(const std::string& username, std::string& error);

} // namespace sdauth