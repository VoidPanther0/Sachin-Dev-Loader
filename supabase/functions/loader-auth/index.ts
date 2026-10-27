import "jsr:@supabase/functions-js/edge-runtime.d.ts";

// loader-auth
// Server-side authentication and release metadata for the Sachin Loader client.
//
// The client sends username + password + HWID only. This function verifies the
// credentials against Supabase Auth, then enforces ban / HWID limits through
// public.account_login(). Supabase tokens never leave this function until the
// account checks pass.
//
// Actions:
//   startup   { hwid, version, exe_hash }                 -> components, news, announcement, release
//   login     { username, password, hwid }                -> session
//   bootstrap { access_token, hwid, version, exe_hash }   -> components + release
//   refresh   { refresh_token, hwid }                     -> session
//   check     { access_token, hwid }                      -> ok
//   logout    { access_token }                            -> ok
//
// The function also mirrors the newest GitHub release into loader_config. That
// keeps the version, download URL and SHA-256 in Supabase identical to the
// published release without a workflow secret or a manual SQL step. The mirror
// runs at most once every five minutes.
//
// Deploy with verify_jwt = false: login and refresh carry no user JWT, and the
// function authorises every request itself.

const SUPABASE_URL = (Deno.env.get("SUPABASE_URL") ?? "").replace(/\/+$/, "");
const API_KEY = ReadEnvKey("SUPABASE_PUBLISHABLE_KEYS") || ReadEnvKey("SUPABASE_ANON_KEY");
const SERVICE_KEY = ReadEnvKey("SUPABASE_SECRET_KEYS") || ReadEnvKey("SUPABASE_SERVICE_ROLE_KEY");

const EMAIL_DOMAIN = "loader.local";
const GITHUB_REPO = "sapkotasachin064-beep/Sachin-Dev-Loader";
const GITHUB_TIMEOUT_MS = 4500;
const SYNC_MEMORY_MS = 60_000;
const MAX_BODY_BYTES = 8192;
const REQUEST_TIMEOUT_MS = 15000;

const CORS_HEADERS: Record<string, string> = {
	"Access-Control-Allow-Origin": "*",
	"Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type",
	"Access-Control-Allow-Methods": "POST, OPTIONS",
};

// Supabase now exposes key sets as JSON objects. Accept both the JSON form and
// the legacy plain string so a project can move between the two formats.
function ReadEnvKey(name: string): string {
	const raw = (Deno.env.get(name) ?? "").trim();
	if (!raw) return "";

	try {
		const parsed = JSON.parse(raw);
		if (typeof parsed === "string") return parsed.trim();
		if (parsed && typeof parsed === "object") {
			const values = parsed as Record<string, unknown>;
			for (const key of ["default", "anon", "service_role", "publishable", "secret"]) {
				const value = values[key];
				if (typeof value === "string" && value.trim()) return value.trim();
			}
			for (const value of Object.values(values)) {
				if (typeof value === "string" && value.trim()) return value.trim();
			}
		}
	} catch {
		// Not JSON: the raw value is the key.
	}

	return raw;
}

function Json(body: unknown, status = 200): Response {
	return new Response(JSON.stringify(body), {
		status,
		headers: {
			...CORS_HEADERS,
			"Content-Type": "application/json; charset=utf-8",
			"Cache-Control": "no-store",
		},
	});
}

function Fail(error: string, status = 200): Response {
	return Json({ ok: false, error }, status);
}

function Text(value: unknown): string {
	return typeof value === "string" ? value : "";
}

function ClientIp(req: Request): string {
	const forwarded = req.headers.get("x-forwarded-for");
	if (forwarded) {
		const first = forwarded.split(",")[0].trim();
		if (first) return first;
	}
	return req.headers.get("cf-connecting-ip")?.trim() ?? "";
}

async function Post(
	path: string,
	body: unknown,
	token = "",
	ip = "",
): Promise<{ status: number; data: Record<string, unknown> | null; failed: boolean }> {
	const controller = new AbortController();
	const timer = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);

	const headers: Record<string, string> = {
		"Content-Type": "application/json",
		"Accept": "application/json",
		"apikey": API_KEY,
	};
	if (token) headers["Authorization"] = `Bearer ${token}`;
	if (ip) headers["x-forwarded-for"] = ip;

	try {
		const response = await fetch(`${SUPABASE_URL}${path}`, {
			method: "POST",
			headers,
			body: JSON.stringify(body),
			signal: controller.signal,
		});

		const text = await response.text();
		let data: Record<string, unknown> | null = null;
		try {
			const parsed = JSON.parse(text);
			if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) {
				data = parsed as Record<string, unknown>;
			}
		} catch {
			data = null;
		}

		return { status: response.status, data, failed: false };
	} catch {
		return { status: 0, data: null, failed: true };
	} finally {
		clearTimeout(timer);
	}
}

interface ServiceResult {
	status: number;
	data: unknown;
	failed: boolean;
}

async function ServiceCall(path: string, body: unknown): Promise<ServiceResult> {
	if (!SERVICE_KEY) return { status: 0, data: null, failed: true };

	const controller = new AbortController();
	const timer = setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);

	try {
		const response = await fetch(`${SUPABASE_URL}${path}`, {
			method: "POST",
			headers: {
				"Content-Type": "application/json",
				"Accept": "application/json",
				"apikey": SERVICE_KEY,
				"Authorization": `Bearer ${SERVICE_KEY}`,
			},
			body: JSON.stringify(body),
			signal: controller.signal,
		});

		const text = await response.text();
		let data: unknown = null;
		try {
			data = text ? JSON.parse(text) : null;
		} catch {
			data = null;
		}

		return { status: response.status, data, failed: false };
	} catch {
		return { status: 0, data: null, failed: true };
	} finally {
		clearTimeout(timer);
	}
}

function ServiceRpc(name: string, body: unknown): Promise<ServiceResult> {
	return ServiceCall(`/rest/v1/rpc/${name}`, body);
}

function MapAuthError(status: number, data: Record<string, unknown> | null): string {
	const code = Text(data?.error_code) || Text(data?.error);
	const message = Text(data?.msg) || Text(data?.error_description) || Text(data?.message);

	if (status === 429 || code === "over_request_rate_limit" || code === "over_email_send_rate_limit" || code === "too_many_requests") {
		return "Too many attempts. Wait a minute and try again";
	}
	if (code === "invalid_credentials" || code === "invalid_grant" || message === "Invalid login credentials") {
		return "Wrong username or password";
	}
	if (code === "email_not_confirmed") return "Account is not confirmed. Contact the admin";
	if (code === "user_banned") return "Account is banned";
	if (code === "refresh_token_not_found" || code === "refresh_token_already_used" || code === "session_not_found") {
		return "Session expired. Sign in again";
	}
	if (status === 0) return "Cannot reach the server";
	if (status >= 500) return "Server error, try again later";
	if (message) return message;
	return "Login failed. Try again";
}

function MapAccountError(code: string): string {
	switch (code) {
	case "no_account": return "Account setup is incomplete. Contact the admin";
	case "banned": return "This account is banned";
	case "expired": return "Your access has expired. Contact the admin";
	case "hwid_mismatch": return "This account is locked to another device";
	case "rate_limited": return "Too many attempts. Wait a few minutes";
	case "invalid_input": return "Could not read this device ID";
	case "not_authenticated": return "Session expired. Sign in again";
	case "maintenance": return "Loader is under maintenance. Try again later";
	case "outdated": return "This loader version is outdated. Download the latest build";
	case "bad_build": return "Loader file is modified or unknown. Download the official build";
	case "config_missing": return "Loader configuration is missing. Contact the admin";
	default: return "Login failed. Try again";
	}
}

interface Tokens {
	accessToken: string;
	refreshToken: string;
	expiresIn: number;
}

function ReadTokens(data: Record<string, unknown> | null): Tokens | null {
	const accessToken = Text(data?.access_token);
	if (!accessToken) return null;

	const rawExpires = data?.expires_in;
	const expiresIn = typeof rawExpires === "number" && rawExpires > 0 ? Math.floor(rawExpires) : 3600;

	return {
		accessToken,
		refreshToken: Text(data?.refresh_token),
		expiresIn,
	};
}

function SessionBody(username: string, tokens: Tokens): Record<string, unknown> {
	return {
		ok: true,
		username,
		token_type: "bearer",
		access_token: tokens.accessToken,
		refresh_token: tokens.refreshToken,
		expires_in: tokens.expiresIn,
	};
}

async function Revoke(token: string): Promise<void> {
	if (!token) return;
	await Post("/auth/v1/logout", {}, token);
}

async function AccountCall(
	action: "account_login" | "account_status",
	token: string,
	hwid: string,
	ip: string,
): Promise<{ ok: boolean; username: string; error: string; status: number }> {
	const response = await Post(`/rest/v1/rpc/${action}`, { p_hwid: hwid }, token, ip);

	if (response.failed) {
		return { ok: false, username: "", error: "Cannot reach the server", status: 0 };
	}
	if (response.status === 401) {
		return { ok: false, username: "", error: "Session expired. Sign in again", status: 401 };
	}
	if (response.status !== 200 || !response.data) {
		const message = Text(response.data?.message) || Text(response.data?.msg);
		return {
			ok: false,
			username: "",
			error: message || (response.status >= 500 ? "Server error, try again later" : "Login failed. Try again"),
			status: response.status,
		};
	}

	const data = response.data;
	if (data.ok !== true) {
		const code = Text(data.error);
		return { ok: false, username: "", error: MapAccountError(code), status: 403 };
	}

	return { ok: true, username: Text(data.username), error: "", status: 200 };
}

async function HandleLogin(body: Record<string, unknown>, ip: string): Promise<Response> {
	const username = Text(body.username).trim().toLowerCase();
	const password = Text(body.password);
	const hwid = Text(body.hwid).trim().toUpperCase();

	if (!/^[a-z0-9_]{2,20}$/.test(username)) {
		return Fail("Invalid username. Use 2-20 lowercase letters, digits or _");
	}
	if (!password || password.length > 200) {
		return Fail("Wrong username or password");
	}
	if (hwid.length < 8 || hwid.length > 80) {
		return Fail("Could not read this device ID");
	}

	const auth = await Post(
		"/auth/v1/token?grant_type=password",
		{ email: `${username}@${EMAIL_DOMAIN}`, password },
		"",
		ip,
	);

	if (auth.failed) return Fail("Cannot reach the server");
	if (auth.status !== 200) return Fail(MapAuthError(auth.status, auth.data));

	const tokens = ReadTokens(auth.data);
	if (!tokens) return Fail("Login failed. Try again");

	const account = await AccountCall("account_login", tokens.accessToken, hwid, ip);
	if (!account.ok) {
		await Revoke(tokens.accessToken);
		return Fail(account.error);
	}

	return Json(SessionBody(account.username || username, tokens));
}

async function HandleRefresh(body: Record<string, unknown>, ip: string): Promise<Response> {
	const refreshToken = Text(body.refresh_token);
	const hwid = Text(body.hwid).trim().toUpperCase();

	if (!refreshToken) return Fail("Session expired. Sign in again");
	if (hwid.length < 8 || hwid.length > 80) return Fail("Could not read this device ID");

	const auth = await Post(
		"/auth/v1/token?grant_type=refresh_token",
		{ refresh_token: refreshToken },
		"",
		ip,
	);

	if (auth.failed) return Fail("Cannot reach the server");
	if (auth.status !== 200) return Fail(MapAuthError(auth.status, auth.data));

	const tokens = ReadTokens(auth.data);
	if (!tokens) return Fail("Session expired. Sign in again");

	const account = await AccountCall("account_status", tokens.accessToken, hwid, ip);
	if (!account.ok) {
		await Revoke(tokens.accessToken);
		return Fail(account.error);
	}

	return Json(SessionBody(account.username, tokens));
}

async function HandleCheck(body: Record<string, unknown>, ip: string): Promise<Response> {
	const accessToken = Text(body.access_token);
	const hwid = Text(body.hwid).trim().toUpperCase();

	if (!accessToken) return Fail("Session expired. Sign in again");
	if (hwid.length < 8 || hwid.length > 80) return Fail("Could not read this device ID");

	const account = await AccountCall("account_status", accessToken, hwid, ip);
	if (!account.ok) return Fail(account.error);

	return Json({ ok: true, username: account.username });
}

interface BuildInput {
	hwid: string;
	version: string;
	exeHash: string;
}

function ReadBuild(body: Record<string, unknown>): BuildInput | string {
	const hwid = Text(body.hwid).trim().toUpperCase();
	const version = Text(body.version).trim();
	const exeHash = Text(body.exe_hash).trim().toLowerCase();

	if (hwid.length < 8 || hwid.length > 80) return "Could not read this device ID";
	if (!/^\d{1,4}(\.\d{1,4}){0,3}$/.test(version)) return MapAccountError("outdated");
	if (!/^[0-9a-f]{64}$/.test(exeHash)) return MapAccountError("bad_build");
	return { hwid, version, exeHash };
}

async function BuildCall(
	action: "loader_startup" | "loader_bootstrap",
	build: BuildInput,
	token: string,
	ip: string,
): Promise<Response> {
	const body = { p_hwid: build.hwid, p_version: build.version, p_exe_hash: build.exeHash };
	const response = await Post(`/rest/v1/rpc/${action}`, body, token, ip);

	if (response.failed) return Fail("Cannot reach the server");
	if (response.status === 401) return Fail("Session expired. Sign in again");
	if (response.status !== 200 || !response.data) {
		return Fail(response.status >= 500 ? "Server error, try again later" : "Loader check failed. Try again");
	}

	const data = response.data;
	if (data.ok !== true) {
		const code = Text(data.error);
		const message = Text(data.message);
		const error = message || MapAccountError(code);
		const updateUrl = Text(data.update_url);
		const releaseUrl = Text(data.release_url) || Text(data.url);
		const sha256 = Text(data.sha256);
		const latestVersion = Text(data.latest_version);

		if (updateUrl || releaseUrl) {
			return Json({
				ok: false,
				code,
				error,
				download_url: updateUrl || releaseUrl,
				update_url: updateUrl,
				release_url: releaseUrl,
				sha256,
				latest_version: latestVersion,
			});
		}
		return Fail(error);
	}

	return Json({
		ok: true,
		username: Text(data.username),
		expires_at: typeof data.expires_at === "number" ? data.expires_at : 0,
		components: Array.isArray(data.components) ? data.components : [],
		news: Array.isArray(data.news) ? data.news : [],
		announcement: data.announcement && typeof data.announcement === "object" ? data.announcement : null,
		support_url: Text(data.support_url),
		latest_version: Text(data.latest_version),
		update_url: Text(data.update_url),
		release_url: Text(data.release_url) || Text(data.url),
		sha256: Text(data.sha256),
	});
}

// ------------------------------------------------------------ release sync ----

interface ReleaseInfo {
	version: string;
	updateUrl: string;
	sha256: string;
	releaseUrl: string;
}

let lastSyncAttempt = 0;

async function FetchLatestRelease(): Promise<ReleaseInfo | null> {
	const controller = new AbortController();
	const timer = setTimeout(() => controller.abort(), GITHUB_TIMEOUT_MS);
	const headers = {
		"Accept": "application/vnd.github+json",
		"User-Agent": "SachinDevLoader-release-sync",
	};

	try {
		const response = await fetch(`https://api.github.com/repos/${GITHUB_REPO}/releases/latest`, {
			headers,
			signal: controller.signal,
		});
		if (!response.ok) return null;

		const release = await response.json() as Record<string, unknown>;
		const version = Text(release.tag_name).trim().replace(/^v/i, "");
		if (!/^\d{1,4}(\.\d{1,4}){0,3}$/.test(version)) return null;

		const assets = Array.isArray(release.assets) ? release.assets as Record<string, unknown>[] : [];
		const exe = assets.find((asset) => Text(asset.name) === "ImGuiTemplate.exe");
		const sums = assets.find((asset) => Text(asset.name) === "SHA256SUMS.txt");
		if (!exe || !sums) return null;

		const updateUrl = Text(exe.browser_download_url);
		const sumsUrl = Text(sums.browser_download_url);
		if (!updateUrl.startsWith("https://") || !sumsUrl.startsWith("https://")) return null;

		const sumsResponse = await fetch(sumsUrl, { headers, signal: controller.signal });
		if (!sumsResponse.ok) return null;

		const sumsText = await sumsResponse.text();
		const match = sumsText.match(/^([0-9a-fA-F]{64})\s+\*?ImGuiTemplate\.exe\s*$/m);
		if (!match) return null;

		return {
			version,
			updateUrl,
			sha256: match[1].toLowerCase(),
			releaseUrl: Text(release.html_url),
		};
	} catch {
		return null;
	} finally {
		clearTimeout(timer);
	}
}

async function SyncRelease(): Promise<void> {
	if (!SERVICE_KEY) return;

	const now = Date.now();
	if (now - lastSyncAttempt < SYNC_MEMORY_MS) return;
	lastSyncAttempt = now;

	try {
		const begin = await ServiceRpc("loader_sync_begin", {});
		if (begin.failed || begin.data !== true) return;

		const release = await FetchLatestRelease();
		if (!release) return;

		await ServiceRpc("loader_sync_release", {
			p_version: release.version,
			p_update_url: release.updateUrl,
			p_sha256: release.sha256,
			p_release_url: release.releaseUrl,
		});
	} catch {
		// Keep the last known release when GitHub or the database is unavailable.
	}
}

async function HandleStartup(body: Record<string, unknown>, ip: string): Promise<Response> {
	const build = ReadBuild(body);
	if (typeof build === "string") return Fail(build);
	await SyncRelease();
	return await BuildCall("loader_startup", build, "", ip);
}

async function HandleBootstrap(body: Record<string, unknown>, ip: string): Promise<Response> {
	const accessToken = Text(body.access_token);
	if (!accessToken) return Fail("Session expired. Sign in again");

	const build = ReadBuild(body);
	if (typeof build === "string") return Fail(build);
	await SyncRelease();
	return await BuildCall("loader_bootstrap", build, accessToken, ip);
}

async function HandleLogout(body: Record<string, unknown>): Promise<Response> {
	const accessToken = Text(body.access_token);
	if (accessToken) await Revoke(accessToken);
	return Json({ ok: true });
}

Deno.serve(async (req: Request) => {
	if (req.method === "OPTIONS") {
		return new Response("ok", { headers: CORS_HEADERS });
	}
	if (req.method !== "POST") {
		return Fail("Method not allowed", 405);
	}
	if (!SUPABASE_URL || !API_KEY) {
		return Fail("Server is not configured", 500);
	}

	const declaredLength = Number(req.headers.get("content-length") ?? "0");
	if (declaredLength > MAX_BODY_BYTES) {
		return Fail("Request is too large", 413);
	}

	let body: Record<string, unknown>;
	try {
		const raw = await req.text();
		if (raw.length > MAX_BODY_BYTES) return Fail("Request is too large", 413);
		const parsed = raw ? JSON.parse(raw) : {};
		if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
			return Fail("Bad request", 400);
		}
		body = parsed as Record<string, unknown>;
	} catch {
		return Fail("Bad request", 400);
	}

	const ip = ClientIp(req);
	const action = Text(body.action).trim().toLowerCase() || "login";

	switch (action) {
	case "login":
		return await HandleLogin(body, ip);
	case "refresh":
		return await HandleRefresh(body, ip);
	case "check":
		return await HandleCheck(body, ip);
	case "logout":
		return await HandleLogout(body);
	case "startup":
		return await HandleStartup(body, ip);
	case "bootstrap":
		return await HandleBootstrap(body, ip);
	default:
		return Fail("Unknown action", 400);
	}
});
