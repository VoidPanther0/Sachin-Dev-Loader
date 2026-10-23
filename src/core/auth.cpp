#include "auth.h"

#include "json.h"
#include "paths.h"
#include "sha256.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace auth {

namespace {

struct Account
{
	std::string username;
	std::string salt;
	std::string hash;
};

std::vector<Account> g_accounts;
bool g_loaded = false;

std::string random_salt()
{
	unsigned char bytes[16];
	for (unsigned char& b : bytes)
		b = static_cast<unsigned char>(std::rand() & 0xff);

	char buffer[3];
	std::string result;
	for (unsigned char b : bytes)
	{
		std::snprintf(buffer, sizeof(buffer), "%02x", b);
		result += buffer;
	}
	return result;
}

Account* find(const std::string& username)
{
	for (Account& account : g_accounts)
		if (account.username == username)
			return &account;
	return nullptr;
}

bool save()
{
	json::Value root = json::Value::MakeObject();
	json::Value list = json::Value::MakeArray();
	for (const Account& account : g_accounts)
	{
		json::Value entry = json::Value::MakeObject();
		entry.set("username", account.username);
		entry.set("salt", account.salt);
		entry.set("hash", account.hash);
		list.push_back(entry);
	}
	root.set("accounts", list);
	return json::save_file(paths::accounts_file(), root);
}

} // namespace

void load()
{
	g_accounts.clear();
	g_loaded = true;

	if (!paths::file_exists(paths::accounts_file()))
		return;

	json::Value root;
	if (!json::load_file(paths::accounts_file(), root))
		return;

	for (const json::Value& item : root.get("accounts").as_array())
	{
		Account account;
		account.username = item.get("username").as_string();
		account.salt = item.get("salt").as_string();
		account.hash = item.get("hash").as_string();
		if (!account.username.empty())
			g_accounts.push_back(account);
	}
}

bool account_exists(const std::string& username)
{
	if (!g_loaded) load();
	return find(username) != nullptr;
}

Result register_account(const std::string& username, const std::string& password)
{
	if (!g_loaded) load();

	if (username.empty() || password.empty())
		return { false, "Username and password are required." };
	if (password.size() < 4)
		return { false, "Password must be at least 4 characters." };
	if (find(username))
		return { false, "That username is already registered on this machine." };

	Account account;
	account.username = username;
	account.salt = random_salt();
	account.hash = sha256::hex(account.salt + password);
	g_accounts.push_back(account);

	if (!save())
		return { false, "Could not write the local account file." };

	return { true, "Account created. You can now sign in." };
}

Result login(const std::string& username, const std::string& password)
{
	if (!g_loaded) load();

	Account* account = find(username);
	if (!account)
		return { false, "No local account with that username. Try registering first." };

	if (sha256::hex(account->salt + password) != account->hash)
		return { false, "Incorrect password." };

	return { true, "Signed in." };
}

} // namespace auth
