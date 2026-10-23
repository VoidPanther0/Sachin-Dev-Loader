#pragma once

#include <string>

// Local demo authentication only: accounts and password hashes live in a
// JSON file on this machine. There is no server, no transport security, and
// no session token. Swap this module for a real HTTPS backend before this
// is anything but a local preview.
namespace auth {

struct Result
{
	bool ok = false;
	std::string message;
};

void load();

Result register_account(const std::string& username, const std::string& password);
Result login(const std::string& username, const std::string& password);
bool account_exists(const std::string& username);

} // namespace auth
