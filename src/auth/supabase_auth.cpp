#include "supabase_auth.h"
#include "json_lite.h"
#include "app.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <atomic>
#include <cctype>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
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
const wchar_t* kAuthFunctionPath = L"/functions/v1/loader-auth";
const char* kPublishableKey = "sb_publishable_xoEwnFlQuy-LLO5h8CRW_w_lS_uiEYr";
const char* kUserAgent = "SachinDevLoader/1.0";

const int kHeartbeatSeconds = 300;
const std::size_t kMaxResponseBytes = 256 * 1024;

// ----------------------------------------------------------------- state ----

std::mutex g_mutex;
std::atomic<int> g_status(static_cast<int>(sdauth::StatusIdle));
std::atomic<bool> g_busy(false);
std::atomic<long long> g_next_check(0);
std::atomic<int> g_stage(static_cast<int>(sdauth::StageNone));
std::string g_message;
sdauth::Session g_session;
std::vector<sdauth::Component> g_components;
std::string g_hwid;
std::string g_exe_hash;
std::atomic<int> g_startup_status(static_cast<int>(sdauth::StatusIdle));
std::atomic<int> g_startup_stage(static_cast<int>(sdauth::StartupNone));
std::atomic<bool> g_startup_busy(false);
std::string g_startup_message;

long long NowSeconds() {
	return static_cast<long long>(std::time(nullptr));
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

void SetStage(int stage) {
	g_stage.store(stage);
}

void SetComponents(const std::vector<sdauth::Component>& components) {
	std::lock_guard<std::mutex> lock(g_mutex);
	g_components = components;
}

// Zeros sensitive buffers on every return path.
struct ZeroOnExit {
	explicit ZeroOnExit(std::string& value) : value_(value) {}

	~ZeroOnExit() {
		if (!value_.empty())
			SecureZeroMemory(value_.data(), value_.size());
	}

	std::string& value_;
};

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

// SHA-256 of the running executable. The server compares it with the list of
// released builds, so a patched binary fails the bootstrap check.
const std::string& ExecutableHash() {
	std::lock_guard<std::mutex> lock(g_mutex);
	if (!g_exe_hash.empty())
		return g_exe_hash;

	wchar_t path[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
		return g_exe_hash;

	HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return g_exe_hash;

	LARGE_INTEGER size = {};
	std::string bytes;
	if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < 512LL * 1024 * 1024) {
		bytes.resize(static_cast<std::size_t>(size.QuadPart));
		DWORD read = 0;
		if (!ReadFile(file, &bytes[0], static_cast<DWORD>(bytes.size()), &read, nullptr) || read != bytes.size())
			bytes.clear();
	}
	CloseHandle(file);

	if (!bytes.empty())
		g_exe_hash = Sha256Hex(bytes);
	return g_exe_hash;
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
	const std::wstring agent = Utf8ToWide(kUserAgent);

	HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
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

std::vector<std::wstring> FunctionHeaders() {
	std::vector<std::wstring> headers;
	headers.push_back(L"apikey: " + Utf8ToWide(kPublishableKey));
	headers.push_back(L"Content-Type: application/json");
	headers.push_back(L"Accept: application/json");
	return headers;
}

// The Edge Function returns friendly text in "error" / "message"; use it as-is.
std::string ErrorMessage(const HttpResponse& response, const char* fallback) {
	jsonlite::Value root;
	if (jsonlite::Parse(response.body, root)) {
		std::string message = root.get_string("error");
		if (message.empty())
			message = root.get_string("message");
		if (message.empty())
			message = root.get_string("msg");
		if (!message.empty())
			return message;
	}

	if (response.status == 0)
		return "Cannot reach the server";
	if (response.status >= 500)
		return "Server error, try again later";
	return fallback;
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
	session.expires_at = NowSeconds() + static_cast<long long>(expires_in);
	return true;
}

bool ParseFunctionResult(const std::string& body, std::string& error) {
	jsonlite::Value root;
	if (!jsonlite::Parse(body, root)) {
		error = "Bad server answer";
		return false;
	}

	if (!root.get_bool("ok", false)) {
		error = root.get_string("error");
		if (error.empty())
			error = "Account check failed. Sign in again";
		return false;
	}
	return true;
}

bool ParseComponents(const std::string& body, std::vector<sdauth::Component>& components, std::string& error) {
	jsonlite::Value root;
	if (!jsonlite::Parse(body, root)) {
		error = "Bad server answer";
		return false;
	}

	if (!root.get_bool("ok", false)) {
		error = root.get_string("error");
		if (error.empty())
			error = "Loader check failed. Try again";
		return false;
	}

	const jsonlite::Value* list = root.find("components");
	if (list && list->is_array()) {
		for (const jsonlite::Value& item : list->array) {
			sdauth::Component component;
			component.name = item.get_string("name");
			component.version = item.get_string("version");
			component.description = item.get_string("description");
			if (!component.name.empty())
				components.push_back(component);
		}
	}
	return true;
}

// Server-side bootstrap: account, HWID, client version and executable hash are
// checked again with the fresh token before any component is released.
bool RunBootstrap(const std::string& access_token, const std::string& hwid, std::string& error) {
	const std::string exe_hash = ExecutableHash();
	if (exe_hash.empty()) {
		error = "Cannot read the loader file";
		return false;
	}

	std::string body =
		"{\"action\":\"bootstrap\",\"access_token\":\"" + JsonEscape(access_token) +
		"\",\"hwid\":\"" + JsonEscape(hwid) +
		"\",\"version\":\"" + JsonEscape(app::kVersion) +
		"\",\"exe_hash\":\"" + JsonEscape(exe_hash) + "\"}";
	ZeroOnExit wipe_body(body);

	HttpResponse response = HttpPost(ApiUrl(kAuthFunctionPath), FunctionHeaders(), body);
	ZeroOnExit wipe_response(response.body);

	if (!response.transport_ok) {
		error = response.error.empty() ? "Cannot reach the server" : response.error;
		return false;
	}

	std::vector<sdauth::Component> components;
	if (response.status != 200 || !ParseComponents(response.body, components, error)) {
		if (error.empty() || response.status != 200)
			error = ErrorMessage(response, "Loader check failed. Try again");
		return false;
	}

	SetComponents(components);
	return true;
}

// --------------------------------------------------------------- startup ----

void SetStartup(int status, const std::string& message) {
	{
		std::lock_guard<std::mutex> lock(g_mutex);
		g_startup_message = message;
	}
	g_startup_status.store(status);
}

// Runs before the login form. No account yet: the server checks the build
// and maintenance flag and returns the public component list.
void RunStartup(std::string hwid) {
	g_startup_stage.store(sdauth::StartupConnecting);

	const std::string exe_hash = ExecutableHash();
	if (exe_hash.empty()) {
		SetStartup(sdauth::StatusError, "Cannot read the loader file");
		return;
	}

	std::string body =
		"{\"action\":\"startup\",\"hwid\":\"" + JsonEscape(hwid) +
		"\",\"version\":\"" + JsonEscape(app::kVersion) +
		"\",\"exe_hash\":\"" + JsonEscape(exe_hash) + "\"}";

	const HttpResponse response = HttpPost(ApiUrl(kAuthFunctionPath), FunctionHeaders(), body);
	if (!response.transport_ok) {
		SetStartup(sdauth::StatusError, response.error.empty() ? "Cannot reach the server" : response.error);
		return;
	}

	g_startup_stage.store(sdauth::StartupIntegrity);

	std::string error;
	std::vector<sdauth::Component> components;
	if (response.status != 200 || !ParseComponents(response.body, components, error)) {
		if (error.empty() || response.status != 200)
			error = ErrorMessage(response, "Loader check failed. Try again");
		SetStartup(sdauth::StatusError, error);
		return;
	}

	g_startup_stage.store(sdauth::StartupComponents);
	SetComponents(components);

	g_startup_stage.store(sdauth::StartupDone);
	SetStartup(sdauth::StatusOk, "Ready");
}

void RevokeToken(const std::string& token) {
	std::string body = "{\"action\":\"logout\",\"access_token\":\"" + JsonEscape(token) + "\"}";
	ZeroOnExit wipe_body(body);
	HttpResponse response = HttpPost(ApiUrl(kAuthFunctionPath), FunctionHeaders(), body);
	ZeroOnExit wipe_response(response.body);
}

void RunLogin(std::string username, std::string password, std::string hwid) {
	ZeroOnExit wipe_password(password);
	SetStage(sdauth::StageAuthenticating);

	std::string body =
		"{\"action\":\"login\",\"username\":\"" + JsonEscape(username) +
		"\",\"password\":\"" + JsonEscape(password) +
		"\",\"hwid\":\"" + JsonEscape(hwid) + "\"}";
	ZeroOnExit wipe_body(body);

	HttpResponse response = HttpPost(ApiUrl(kAuthFunctionPath), FunctionHeaders(), body);
	ZeroOnExit wipe_response(response.body);

	if (!response.transport_ok) {
		SetStatus(sdauth::StatusError, response.error.empty() ? "Cannot reach the server" : response.error);
		return;
	}

	sdauth::Session session;
	if (response.status != 200 || !ParseSession(response.body, username, session)) {
		SetStatus(sdauth::StatusError, ErrorMessage(response, "Login failed. Try again"));
		return;
	}

	ZeroOnExit wipe_access(session.access_token);
	ZeroOnExit wipe_refresh(session.refresh_token);

	// The login answer already passed account_login (ban, HWID, rate limit).
	SetStage(sdauth::StageAccount);
	SetStage(sdauth::StageSecurity);

	std::string error;
	if (!RunBootstrap(session.access_token, hwid, error)) {
		RevokeToken(session.access_token);
		SetStatus(sdauth::StatusError, error);
		return;
	}

	SetSession(session);
	g_next_check.store(NowSeconds() + kHeartbeatSeconds);
	SetStage(sdauth::StageDone);
	SetStatus(sdauth::StatusOk, "Welcome back, " + session.username);
}

// --------------------------------------------------------------- heartbeat ----

bool RefreshSession(std::string& error) {
	const sdauth::Session current = sdauth::GetSession();
	if (!current.valid || current.refresh_token.empty()) {
		error = "Session expired. Sign in again";
		return false;
	}

	std::string body =
		"{\"action\":\"refresh\",\"refresh_token\":\"" + JsonEscape(current.refresh_token) +
		"\",\"hwid\":\"" + JsonEscape(sdauth::MachineHwid()) + "\"}";
	ZeroOnExit wipe_body(body);

	HttpResponse response = HttpPost(ApiUrl(kAuthFunctionPath), FunctionHeaders(), body);
	ZeroOnExit wipe_response(response.body);
	if (!response.transport_ok) {
		error = response.error.empty() ? "Cannot reach the server" : response.error;
		return false;
	}

	sdauth::Session refreshed;
	if (response.status != 200 || !ParseSession(response.body, current.username, refreshed)) {
		error = ErrorMessage(response, "Session expired. Sign in again");
		return false;
	}

	ZeroOnExit wipe_access(refreshed.access_token);
	ZeroOnExit wipe_refresh(refreshed.refresh_token);

	SetSession(refreshed);
	return true;
}

void RunHeartbeat() {
	sdauth::Session current = sdauth::GetSession();
	if (!current.valid) {
		g_busy.store(false);
		return;
	}

	if (current.expires_at - 60 <= NowSeconds()) {
		std::string refresh_error;
		if (!RefreshSession(refresh_error)) {
			const bool had_session = sdauth::HasSession();
			sdauth::SignOut();
			if (had_session)
				SetStatus(sdauth::StatusError, refresh_error.empty() ? "Session expired. Sign in again" : refresh_error);
			g_busy.store(false);
			return;
		}
		current = sdauth::GetSession();
		if (!current.valid) {
			g_busy.store(false);
			return;
		}
	}

	std::string check_body =
		"{\"action\":\"check\",\"access_token\":\"" + JsonEscape(current.access_token) +
		"\",\"hwid\":\"" + JsonEscape(sdauth::MachineHwid()) + "\"}";
	ZeroOnExit wipe_check(check_body);

	const HttpResponse response = HttpPost(ApiUrl(kAuthFunctionPath), FunctionHeaders(), check_body);

	if (!response.transport_ok) {
		g_busy.store(false); // network hiccup: keep the session, retry later
		return;
	}

	std::string error;
	if (response.status != 200 || !ParseFunctionResult(response.body, error)) {
		const bool had_session = sdauth::HasSession();
		sdauth::SignOut();
		if (had_session) {
			if (response.status == 401 && error.empty())
				error = "Session expired. Sign in again";
			SetStatus(sdauth::StatusError, error.empty() ? "Account check failed. Sign in again" : error);
		}
		g_busy.store(false);
		return;
	}

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

void StartLogin(const std::string& username, const std::string& password) {
	const std::string name = NormalizeUsername(username);
	std::string error;

	if (!ValidateUsername(name, error)) {
		ReportLocalError(error);
		return;
	}

	if (password.empty()) {
		ReportLocalError("Enter your password");
		return;
	}

	if (g_busy.exchange(true))
		return;

	const std::string hwid = MachineHwid();
	SetStage(StageAuthenticating);
	SetStatus(StatusWorking, "Signing in...");

	std::thread([name, password, hwid]() mutable {
		RunLogin(name, std::move(password), std::move(hwid));
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

void StartStartup() {
	if (g_startup_busy.exchange(true))
		return;

	const std::string hwid = MachineHwid();
	g_startup_stage.store(StartupConnecting);
	SetStartup(StatusWorking, "Connecting...");

	std::thread([hwid]() {
		RunStartup(hwid);
		g_startup_busy.store(false);
	}).detach();
}

int StartupStatus() {
	return g_startup_status.load();
}

int StartupStageValue() {
	return g_startup_stage.load();
}

std::string StartupMessage() {
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_startup_message;
}

int LoginStage() {
	return g_stage.load();
}

std::vector<Component> Components() {
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_components;
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
	SetStage(StageNone);
	g_next_check.store(0);

	if (!current.valid || current.access_token.empty())
		return;

	// Revoke the session in the background, never block the render loop.
	const std::string token = current.access_token;
	std::thread([token]() { RevokeToken(token); }).detach();
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

std::string NormalizeUsername(const std::string& username) {
	const std::size_t begin = username.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos)
		return std::string();

	const std::size_t end = username.find_last_not_of(" \t\r\n");
	std::string out = username.substr(begin, end - begin + 1);
	for (char& c : out)
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
	return out;
}

} // namespace sdauth
