#include "common/login_common.h"
#include "../auth/supabase_auth.h"

#include <cstdio>
#include <string>

// Loading screens shown in the login window.
//
//   Startup  runs when the exe starts: connect, build check, update check. When
//            the server has a newer build (latest_version above this one) or
//            rejects this one as outdated or modified, this screen shows the
//            update, downloads it, replaces the exe and restarts by itself.
//            Maintenance and network errors stop here too, with the server's
//            message and a Retry button.
//   Signing  runs after the Login button: password check, device binding and
//            the security checks. Every step is decided by the server.
//
// This is the only place the loader updates. The dashboard never shows it.

namespace screen_loading {

namespace {

using namespace ui;
using namespace login_ui;

// Slow enough to read every step; the server answer is never the limit.
constexpr double kStartupStepSeconds = 0.75;
// The update notice stays up before the download starts, and the finished
// notice stays up before the restart.
constexpr double kUpdateNoticeSeconds = 3.0;
constexpr double kRestartCountdownSeconds = 4.0;
constexpr double kUpdateRetrySeconds = 3.0;
// An optional update that keeps failing must not lock the login form.
constexpr int kOptionalUpdateAttempts = 3;

const char* const kStartupSteps[] = {
	"Connecting to server...",
	"Checking loader integrity...",
	"Checking for update...",
};

const char* const kLoginSteps[] = {
	"Authenticating...",
	"Verifying account and device...",
	"Running security checks...",
};

struct LoadingState
{
	LoadingAnim startup;
	LoadingAnim signing;
	bool restart_failed = false;
	double update_notice_time = -1.0;
	double update_ready_time = -1.0;
	double update_retry_time = -1.0;
	int update_failures = 0;
};

LoadingState loading;

void reset_update_ui()
{
	loading.restart_failed = false;
	loading.update_notice_time = -1.0;
	loading.update_ready_time = -1.0;
	loading.update_retry_time = -1.0;
	loading.update_failures = 0;
}

Rect action_button()
{
	const float fx = 60.0f;
	return rect(fx, 472.0f, app::kLoginWidth - fx, 530.0f);
}

// Update notice, download progress and the automatic restart. A failed download
// is retried; a required update retries until it works because the form cannot
// open before this build is replaced, an optional one is dropped after a few
// failures.
void draw_update_panel()
{
	const float w = app::kLoginWidth;
	const float fx = 60.0f;
	const float fw = w - fx * 2.0f;
	const std::string version = sdauth::StartupLatestVersion();
	const int status = sdauth::UpdateStatus();
	const bool installed = status == sdauth::UpdateReady;
	const bool failed = status == sdauth::UpdateFailed;
	const double now = ImGui::GetTime();

	pill(rect(fx, 250.0f, fx + 148.0f, 272.0f),
		installed ? "UPDATE READY" : failed ? "RETRYING UPDATE" : "UPDATE FOUND",
		installed ? kGreen : kLoginSubtitle);

	text(font.title, 26.0f, ImVec2(fx, 292.0f), kText,
		installed ? "Restarting loader" : "Updating loader");

	if (!version.empty())
	{
		char line[80];
		std::snprintf(line, sizeof(line), "New version: v%s", version.c_str());
		text(font.body, 16.0f, ImVec2(fx, 338.0f), kLoginHint, line);
	}
	else
	{
		text(font.body, 16.0f, ImVec2(fx, 338.0f), kLoginHint, "A newer loader build is required.");
	}
	text(font.body, 16.0f, ImVec2(fx, 362.0f), kLoginHint,
		installed ? "The latest build is installed." : "It installs automatically, then the loader restarts.");

	DrawBar(rect(fx, 410.0f, fx + fw, 418.0f), static_cast<float>(sdauth::UpdateProgress()) / 100.0f);

	std::string caption = sdauth::UpdateMessage();
	if (loading.restart_failed)
		caption = "The restart did not start. Use the button below.";
	else if (failed)
		caption = "Download did not finish. Retrying automatically...";
	else if (status == sdauth::UpdateIdle)
		caption = "Update found. Starting the download...";
	else if (caption.empty())
		caption = "Downloading update...";
	text(font.body, 15.0f, ImVec2(fx, 426.0f), kLoginHint, caption.c_str());

	const Rect button = action_button();

	if (installed)
	{
		loading.update_retry_time = -1.0;
		if (loading.update_ready_time < 0.0)
		{
			loading.update_ready_time = now;
			loading.restart_failed = false;
		}

		if (loading.restart_failed)
		{
			if (GradientButton("##update_restart", button, "Retry restart"))
			{
				loading.restart_failed = !sdauth::RestartLoader();
				if (!loading.restart_failed)
					PostQuitMessage(0);
			}
			return;
		}

		const double elapsed = now - loading.update_ready_time;
		if (elapsed >= kRestartCountdownSeconds)
		{
			loading.restart_failed = !sdauth::RestartLoader();
			if (!loading.restart_failed)
				PostQuitMessage(0);
			loading.update_ready_time = -1.0;
			return;
		}

		int remaining = static_cast<int>(kRestartCountdownSeconds - elapsed);
		if (remaining < 1)
			remaining = 1;
		char label[64];
		std::snprintf(label, sizeof(label), "Restarting in %d...", remaining);
		BusyButton(button, label);
		return;
	}

	loading.update_ready_time = -1.0;

	if (failed)
	{
		if (loading.update_retry_time < 0.0)
			loading.update_retry_time = now + kUpdateRetrySeconds;

		const double wait = loading.update_retry_time - now;
		if (wait <= 0.0)
		{
			loading.update_retry_time = -1.0;
			++loading.update_failures;
			sdauth::StartUpdate();
			BusyButton(button, "Retrying update...");
			return;
		}

		int remaining = static_cast<int>(wait + 0.999);
		if (remaining < 1)
			remaining = 1;
		char label[64];
		std::snprintf(label, sizeof(label), "Retrying in %d...", remaining);
		BusyButton(button, label);
		return;
	}

	// Idle: hold the notice so it can be read, then start the download.
	if (status == sdauth::UpdateIdle)
	{
		if (loading.update_notice_time < 0.0)
			loading.update_notice_time = now;

		const double wait = kUpdateNoticeSeconds - (now - loading.update_notice_time);
		if (wait <= 0.0)
		{
			sdauth::StartUpdate();
			BusyButton(button, "Starting...");
			return;
		}

		int remaining = static_cast<int>(wait + 0.999);
		if (remaining < 1)
			remaining = 1;
		char label[64];
		std::snprintf(label, sizeof(label), "Updating in %d...", remaining);
		BusyButton(button, label);
		return;
	}

	BusyButton(button, "Updating...");
}

} // namespace

void BeginStartup()
{
	StartAnim(loading.startup);
	reset_update_ui();
	sdauth::StartStartup();
}

Result Startup()
{
	const int count = static_cast<int>(sizeof(kStartupSteps) / sizeof(kStartupSteps[0]));
	const int status = sdauth::StartupStatus();
	const int stage = sdauth::StartupStageValue();
	const bool failed = status == sdauth::StatusError;
	const bool ready = status == sdauth::StatusOk;

	// StartupConnecting (1) is step 0; StartupDone means every step is done.
	const int real_active = ready ? count : stage - 1;
	if (!failed)
		Advance(loading.startup, real_active, count, kStartupStepSeconds);

	// A newer build, or one the server rejects as outdated or modified, updates
	// here once every step has been shown, so the steps stay readable.
	const bool answered = (ready && Finished(loading.startup)) || (failed && sdauth::StartupUpdateRequired());
	if (answered && sdauth::StartupUpdateAvailable() && !sdauth::StartupDownloadUrl().empty())
	{
		// An optional update that keeps failing is dropped so login stays open.
		if (!sdauth::StartupUpdateRequired() && loading.update_failures >= kOptionalUpdateAttempts &&
			sdauth::UpdateStatus() == sdauth::UpdateFailed)
		{
			sdauth::SkipUpdate();
		}
		else
		{
			draw_update_panel();
			return Result::Running;
		}
	}

	const int failed_step = failed ? (loading.startup.shown < count ? loading.startup.shown : count - 1) : -1;
	std::string caption = failed ? sdauth::StartupMessage() : std::string("Preparing loader...");
	if (caption.empty())
		caption = failed ? "Startup check failed. Retry." : "Preparing loader...";

	DrawLoading(kStartupSteps, count, loading.startup, failed_step, caption, failed);

	if (failed)
	{
		if (GradientButton("##startup_retry", action_button(), "Retry"))
			BeginStartup();
		return Result::Failed;
	}

	BusyButton(action_button(), "Loading...");

	if (Finished(loading.startup))
		return Result::Done;
	return Result::Running;
}

void BeginSigning()
{
	StartAnim(loading.signing);
}

Result Signing()
{
	const int count = static_cast<int>(sizeof(kLoginSteps) / sizeof(kLoginSteps[0]));
	const int status = sdauth::LoginStatus();

	// The caller shows the server's reason on the form. Nothing was unlocked.
	if (status == sdauth::StatusError)
		return Result::Failed;

	const bool ok = status == sdauth::StatusOk && sdauth::HasSession();
	const int real_active = ok ? count : sdauth::LoginStage() - 1;
	Advance(loading.signing, real_active, count);

	DrawLoading(kLoginSteps, count, loading.signing, -1, "Signing in...", false);
	BusyButton(action_button(), "Please wait");

	if (ok && Finished(loading.signing))
		return Result::Done;
	return Result::Running;
}

} // namespace screen_loading
