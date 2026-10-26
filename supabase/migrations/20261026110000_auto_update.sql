-- Automatic loader updates and two-character usernames.
--
-- loader_check_build() now returns the direct executable URL, its SHA-256 and
-- the GitHub release page. The client downloads the executable, verifies the
-- hash and replaces itself instead of opening a browser.
--
-- The username rule is relaxed to 2-20 characters so short names like "fz"
-- work on old and new clients.

alter table public.profiles
	drop constraint if exists profiles_username_format;

alter table public.profiles
	add constraint profiles_username_format
	check (username = lower(username) and username ~ '^[a-z0-9_]{2,20}$');

alter table public.loader_config
	add column if not exists update_url text not null
	default 'https://github.com/sapkotasachin064-beep/Sachin-Dev-Loader/releases/latest/download/ImGuiTemplate.exe';

alter table public.loader_config
	add column if not exists update_sha256 text;

update public.loader_config
set min_version = '0.1',
	release_url = 'https://github.com/sapkotasachin064-beep/Sachin-Dev-Loader/releases/latest',
	update_url = 'https://github.com/sapkotasachin064-beep/Sachin-Dev-Loader/releases/latest/download/ImGuiTemplate.exe'
where id = 1;

create or replace function public.loader_check_build(p_version text, p_exe_hash text)
returns jsonb
language plpgsql
stable
security definer
set search_path = public
as $$
declare
	v_config public.loader_config;
	v_parts int[];
begin
	select * into v_config from public.loader_config where id = 1;

	if v_config.maintenance then
		return jsonb_build_object('ok', false, 'error', 'maintenance', 'message', v_config.maintenance_message);
	end if;

	v_parts := public.loader_version_parts(p_version);
	if v_parts is null or v_parts < public.loader_version_parts(v_config.min_version) then
		return jsonb_build_object(
			'ok', false,
			'error', 'outdated',
			'url', v_config.release_url,
			'release_url', v_config.release_url,
			'update_url', v_config.update_url,
			'sha256', v_config.update_sha256
		);
	end if;

	if p_exe_hash is null or p_exe_hash !~ '^[0-9a-f]{64}$' then
		return jsonb_build_object(
			'ok', false,
			'error', 'bad_build',
			'url', v_config.release_url,
			'release_url', v_config.release_url,
			'update_url', v_config.update_url,
			'sha256', v_config.update_sha256
		);
	end if;

	if v_config.enforce_build_hash and not exists (
		select 1 from public.loader_builds b where b.exe_hash = p_exe_hash and b.active
	) then
		return jsonb_build_object(
			'ok', false,
			'error', 'bad_build',
			'url', v_config.release_url,
			'release_url', v_config.release_url,
			'update_url', v_config.update_url,
			'sha256', v_config.update_sha256
		);
	end if;

	return null;
end;
$$;

revoke all on function public.loader_check_build(text, text) from public, anon, authenticated;
