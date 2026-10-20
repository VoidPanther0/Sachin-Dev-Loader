#include "supabase_auth.h"
#include "json_lite.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <atomic>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 0x00000800
#endif

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

namespace {

// ---------------------------------------------------------------- config ----

const wchar_t* kBaseUrl = L"https://xdtidxtkvrmurzogkxuv.supabase.co";
const char* kPublishableKey = "sb_publishable_xoEwnFlQuy-LLO5h8CRW_w_lS_uiEYr";
const char* kEmailDomain = "loader.local";
const char* kUserAgent = "SachinDevLoader/1.0";

const int kHeartbeatSeconds = 300;
const std::size_t kMaxResponseBytes = 256 * 1024;

// ----------------------------------------------------------------- state ----

std::mutex g_mutex;
std::atomic<int> g_status(static_cast<int>(sdauth::StatusIdle));
std::atomic<bool> g_busy(false);
std::atomic<long long> g_next_check(0);
std::string g_message;
sdauth::Session g_session;
std::string g_hwid;

long long NowSeconds() {
	return static_cast<long long>(GetTickCount64() / 1000);
}

void SetStatus(int status, const std::string& message) {
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_message = message;
	}
	g_status.store(status);
}

void SetSession(const sdauth::Session& session) {
	std::lock_guard<std::mutex> lock(g_mutex);
	g_session = session;
}

// ----------------------------------------------------------------- utf-8 ----

std::string WideToUtf8(const std::wstring& text) {
	if (text.empty())
		return std::string();

	const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
	std::string out(static_cast<std::size_t>(size), '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &out[0], size, nullptr, nullptr);
	return out;
}

std::wstring Utf8ToWide(const std::string& text) {
	if (text.empty())
		return std::wstring();

	const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
	std::wstring out(static_cast<std::size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &out[0], size);
	return out;
}

std::string JsonEscape(const std::string& text) {
	std::string out;
	out.reserve(text.size() + 8);

	for (const char raw : text) {
		const unsigned char c = static_cast<unsigned char>(raw);
		switch (c) {
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c < 0x20) {
				char buffer[8];
				sprintf_s(buffer, "\\u%04x", c);
				out += buffer;
			}
			else {
				out += raw;
			}
		}
	}
	return out;
}

// ---------------------------------------------------------------- hashing ----

std::string Sha256Hex(const std::string& data) {
	BCRYPT_ALG_HANDLE algorithm = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	std::string result;

	if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
		return result;

	DWORD object_size = 0;
	DWORD hash_size = 0;
	DWORD written = 0;

	const bool sized =
		BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &written, 0) == 0 &&
		BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size), &written, 0) == 0;

	if (sized) {
		std::vector<unsigned char> object(object_size);
		std::vector<unsigned char> digest(hash_size);

		if (BCryptCreateHash(algorithm, &hash, &object[0], object_size, nullptr, 0, 0) == 0) {
			if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())), static_cast<ULONG>(data.size()), 0) == 0 &&
				BCryptFinishHash(hash, &digest[0], hash_size, 0) == 0) {
				static const char* digits = "0123456789abcdef";
				result.reserve(digest.size() * 2);
				for (const unsigned char byte : digest) {
					result += digits[byte >> 4];
					result += digits[byte & 0x0F];
				}
			}
			BCryptDestroyHash(hash);
		}
	}

	BCryptCloseAlgorithmProvider(algorithm, 0);
	return result;
}

// ------------------------------------------------------------------- http ----

struct HttpResponse {
	bool transport_ok = false;
	int status = 0;
	std::string body;
	std::string error;
};

HttpResponse HttpPost(const std::wstring& url, const std::vector<std::wstring>& headers, const std::string& body) {
	HttpResponse result;

	URL_COMPONENTS parts;
	ZeroMemory(&parts, sizeof(parts));
	parts.dwStructSize = sizeof(parts);

	wchar_t host[256] = {};
	wchar_t path[2048] = {};
	parts.lpszHostName = host;
	parts.dwHostNameLength = 255;
	parts.lpszUrlPath = path;
	parts.dwUrlPathLength = 2047;

	if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts)) {
		result.error = "Bad server URL";
		return result;
	}

	const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;

	HINTERNET session = WinHttpOpen(Utf8ToWide(kUserAgent).c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) {
		result.error = "WinHTTP is not available";
		return result;
	}

	WinHttpSetTimeouts(session, 8000, 8000, 8000, 20000);

	if (secure) {
		DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
		WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
	}

	HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
	if (!connection) {
		WinHttpCloseHandle(session);
		result.error = "Cannot reach the server";
		return result;
	}

	HINTERNET request = WinHttpOpenRequest(connection, L"POST", path, nullptr, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
	if (!request) {
		WinHttpCloseHandle(connection);
		WinHttpCloseHandle(session);
		result.error = "Cannot create the request";
		return result;
	}

	std::wstring header_block;
	for (const std::wstring& header : headers) {
		header_block += header;
		header_block += L"\r\n";
	}

	const bool sent = WinHttpSendRequest(request, header_block.c_str(), static_cast<DWORD>(header_block.size()),
		body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
		static_cast<DWORD>(body.size()), 0) != FALSE;

	if (sent && WinHttpReceiveResponse(request, nullptr)) {
		DWORD status_code = 0;
		DWORD size = sizeof(status_code);
		WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &size, WINHTTP_NO_HEADER_INDEX);

		for (;;) {
			DWORD available = 0;
			if (!WinHttpQueryDataAvailable(request, &available) || available == 0)
				break;

			if (result.body.size() + available > kMaxResponseBytes)
				available = static_cast<DWORD>(kMaxResponseBytes - result.body.size());
			if (available == 0)
				break;

			std::string chunk(available, '\0');
			DWORD read = 0;
			if (!WinHttpReadData(request, &chunk[0], available, &read))
				break;

			chunk.resize(read);
			result.body += chunk;
		}

		result.transport_ok = true;
		result.status = static_cast<int>(status_code);
	}
	else {
		result.error = "Cannot reach the server";
	}

	WinHttpCloseHandle(request);
	WinHttpCloseHandle(connection);
	WinHttpCloseHandle(session);
	return result;
}

std::wstring ApiUrl(const wchar_t* path) {
	return std::wstring(kBaseUrl) + path;
}

std::vector<std::wstring> AuthHeaders(const std::string& bearer) {
	std::vector<std::wstring> headers;
	headers.push_back(L"apikey: " + Utf8ToWide(kPublishableKey));
	headers.push_back(L"Content-Type: application/json");
	headers.push_back(L"Accept: application/json");

	if (!bearer.empty())
		headers.push_back(L"Authorization: Bearer " + Utf8ToWide(bearer));

	return headers;
}

// Reads msg / error_code / message out of a Supabase error body.
void ReadErrorBody(const std::string& body, int status, std::string& code, std::string& message) {
	jsonlite::Value root;
	if (!jsonlite::Parse(body, root))
		return;

	code = root.get_string("error_code");
	if (code.empty())
		code = root.get_string("error");

	message = root.get_string("msg");
	if (message.empty())
		message = root.get_string("error_description");
	if (message.empty())
		message = root.get_string("message");

	if (message.empty() && status >= 500)
		message = "Server error, try again later";
}

std::string MapLoginError(int status, const std::string& code, const std::string& message) {
	if (status == 429 || code == "over_email_send_rate_limit" || code == "too_many_requests")
		return "Too many attempts. Wait a minute and try again";
	if (code == "invalid_credentials" || code == "invalid_grant" || message == "Invalid login credentials")
		return "Wrong username or password";
	if (code == "email_not_confirmed")
		return "Account is not confirmed. Contact the seller";
	if (code == "user_banned")
		return "Account is banned";
	if (code == "email_address_invalid")
		return "That username is not allowed";
	if (status == 0)
		return "Cannot reach the server";
	if (!message.empty())
		return message;

	return "Login failed. Try again";
}

std::string MapLicenseError(const std::string& code) {
	if (code == "invalid_key") return "License key is wrong";
	if (code == "license_disabled") return "License key is disabled";
	if (code == "license_expired") return "License key has expired";
	if (code == "license_in_use") return "License key is used by another account";
	if (code == "hwid_mismatch") return "License locked to another device";
	if (code == "rate_limited") return "Too many attempts. Wait a few minutes";
	if (code == "banned") return "Account is banned";
	if (code == "no_license") return "No license is bound to this account";
	if (code == "invalid_input") return "Fill in every field";
	if (code == "not_authenticated") return "Session expired, sign in again";
	return "License check failed";
}

// ------------------------------------------------------------------ login ----

bool ParseSession(const std::string& body, const std::string& username, sdauth::Session& session) {
	jsonlite::Value root;
	if (!jsonlite::Parse(body, root))
		return false;

	const std::string access = root.get_string("access_token");
	if (access.empty())
		return false;

	const double expires_in = root.get_number("expires_in", 3600.0);

	session.valid = true;
	session.username = username;
	session.access_token = access;
	session.refresh_token = root.get_string("refresh_token");
	session.expires_at = static_cast<long long>(std::time(nullptr)) + static_cast<long long>(expires_in);
	return true;
}

bool ParseLicense(const std::string& body, sdauth::Session& session, std::string& error) {
	jsonlite::Value root;
	if (!jsonlite::Parse(body, root)) {
		error = "Bad server answer";
		return false;
	}

	if (!root.get_bool("ok", false)) {
		error = MapLicenseError(root.get_string("error"));
		return false;
	}

	const jsonlite::Value* license = root.find("license");
	if (license) {
		session.license.label = license->get_string("label");
		session.license.key_prefix = license->get_string("key_prefix");
		session.license.expires_at = license->get_string("expires_at");
		session.license.days_left = static_cast<int>(license->get_number("days_left", 0));
		session.license.hwid = license->get_string("hwid");
	}
	return true;
}

void RunLogin(std::string username, std::string password, std::string key, std::string hwid) {
	const std::string token_body =
		"{\"email\":\"" + JsonEscape(username + "@" + kEmailDomain) + "\",\"password\":\"" + JsonEscape(password) + "\"}";

	const HttpResponse token = HttpPost(ApiUrl(L"/auth/v1/token?grant_type=password"), AuthHeaders(""), token_body);

	if (!token.transport_ok) {
		SetStatus(sdauth::StatusError, token.error.empty() ? "Cannot reach the server" : token.error);
		return;
	}

	sdauth::Session session;
	if (token.status != 200 || !ParseSession(token.body, username, session)) {
		std::string code, message;
		ReadErrorBody(token.body, token.status, code, message);
		SetStatus(sdauth::StatusError, MapLoginError(token.status, code, message));
		return;
	}

	const std::string license_body =
		"{\"p_key\":\"" + JsonEscape(key) + "\",\"p_hwid\":\"" + JsonEscape(hwid) + "\"}";

	const HttpResponse license = HttpPost(ApiUrl(L"/rest/v1/rpc/claim_license"), AuthHeaders(session.access_token), license_body);

	if (!license.transport_ok) {
		SetStatus(sdauth::StatusError, license.error.empty() ? "Cannot reach the server" : license.error);
		return;
	}

	std::string error;
	if (license.status != 200 || !ParseLicense(license.body, session, error)) {
		if (error.empty()) {
			std::string code, message;
			ReadErrorBody(license.body, license.status, code, message);
			error = message.empty() ? "License check failed" : message;
		}
		SetStatus(sdauth::StatusError, error);
		return;
	}

	SetSession(session);
	SetStatus(sdauth::StatusOk, "Welcome back, " + username);
}

// --------------------------------------------------------------- heartbeat ----

bool RefreshSession() {
	sdauth::Session current;
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		current = g_session;
	}

	if (current.refresh_token.empty())
		return false;

	const std::string body = "{\"refresh_token\":\"" + JsonEscape(current.refresh_token) + "\"}";
	const HttpResponse response = HttpPost(ApiUrl(L"/auth/v1/token?grant_type=refresh_token"), AuthHeaders(""), body);

	if (!response.transport_ok || response.status != 200)
		return false;

	sdauth::Session refreshed;
	if (!ParseSession(response.body, current.username, refreshed))
		return false;

	refreshed.license = current.license;
	SetSession(refreshed);
	return true;
}

void RunHeartbeat() {
	sdauth::Session current;
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		current = g_session;
	}

	if (!current.valid) {
		g_busy.store(false);
		return;
	}

	if (current.expires_at - 60 <= static_cast<long long>(std::time(nullptr)) && !RefreshSession()) {
		sdauth::SignOut();
		SetStatus(sdauth::StatusError, "Session expired. Sign in again");
		g_busy.store(false);
		return;
	}

	{
		std::lock_guard<std::mutex> lock(g_mutex);
		current = g_session;
	}

	const HttpResponse response = HttpPost(ApiUrl(L"/rest/v1/rpc/license_status"), AuthHeaders(current.access_token), "{}");

	std::string error;
	sdauth::Session probe = current;
	if (!response.transport_ok) {
		g_busy.store(false); // network hiccup: keep the session, retry later
		return;
	}

	if (response.status != 200 || !ParseLicense(response.body, probe, error)) {
		sdauth::SignOut();
		SetStatus(sdauth::StatusError, error.empty() ? "License check failed. Sign in again" : error);
		g_busy.store(false);
		return;
	}

	probe.valid = true;
	probe.access_token = current.access_token;
	probe.refresh_token = current.refresh_token;
	probe.expires_at = current.expires_at;
	probe.username = current.username;

	if (!HasSession()) { // signed out while the heartbeat was in flight
		g_busy.store(false);
		return;
	}

	SetSession(probe);
	g_busy.store(false);
}

} // namespace

namespace sdauth {

void Init() {
	MachineHwid();
}

void Tick() {
	if (!HasSession())
		return;

	const long long now = NowSeconds();
	if (now < g_next_check.load())
		return;

	if (g_busy.exchange(true))
		return;

	g_next_check.store(now + kHeartbeatSeconds);
	std::thread([]() { RunHeartbeat(); }).detach();
}

void StartLogin(const std::string& username, const std::string& password, const std::string& license_key) {
	if (g_busy.exchange(true))
		return;

	const std::string hwid = MachineHwid();
	SetStatus(StatusWorking, "Signing in...");

	std::thread([username, password, license_key, hwid]() {
		RunLogin(username, password, license_key, hwid);
		g_busy.store(false);
	}).detach();
}

int LoginStatus() {
	return g_status.load();
}

std::string LoginMessage() {
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_message;
}

void ReportLocalError(const std::string& message) {
	SetStatus(StatusError, message);
}

void ResetStatus() {
	g_status.store(StatusIdle);
}

bool HasSession() {
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_session.valid;
}

Session GetSession() {
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_session;
}

void SignOut() {
	const Session current = GetSession();
	SetSession(Session());
	g_next_check.store(0);

	if (!current.valid || current.access_token.empty())
		return;

	// Revoke the session in the background, never block the render loop.
	const std::string token = current.access_token;
	std::thread([token]() {
		HttpPost(ApiUrl(L"/auth/v1/logout"), AuthHeaders(token), "{}");
	}).detach();
}

const std::string& MachineHwid() {
	std::lock_guard<std::mutex> lock(g_mutex);
	if (g_hwid.empty()) {
		DWORD volume_serial = 0;
		GetVolumeInformationW(L"C:\\", nullptr, 0, &volume_serial, nullptr, nullptr, nullptr, 0);

		wchar_t computer_name[256] = {};
		DWORD computer_name_size = 255;
		GetComputerNameW(computer_name, &computer_name_size);

		wchar_t cpu_name[256] = {};
		DWORD cpu_size = sizeof(cpu_name);
		HKEY cpu_key = nullptr;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &cpu_key) == ERROR_SUCCESS) {
			RegQueryValueExW(cpu_key, L"ProcessorNameString", nullptr, nullptr, reinterpret_cast<LPBYTE>(cpu_name), &cpu_size);
			RegCloseKey(cpu_key);
		}

		const std::string raw = std::to_string(volume_serial) + "|" + WideToUtf8(computer_name) + "|" + WideToUtf8(cpu_name);
		const std::string digest = Sha256Hex(raw);

		std::string formatted;
		for (std::size_t i = 0; i + 4 <= digest.size() && formatted.size() < 24; i += 4) {
			if (!formatted.empty())
				formatted += '-';
			formatted += digest.substr(i, 4);
		}

		for (char& c : formatted)
			c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

		g_hwid = formatted;
	}
	return g_hwid;
}

bool ValidateUsername(const std::string& username, std::string& error) {
	if (username.size() < 3 || username.size() > 20) {
		error = "Username must be 3 to 20 characters";
		return false;
	}

	for (const char raw : username) {
		const char c = static_cast<char>(tolower(static_cast<unsigned char>(raw)));
		const bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
		if (!allowed) {
			error = "Username can only use letters, digits and _";
			return false;
		}
	}
	return true;
}

} // namespace sdauth