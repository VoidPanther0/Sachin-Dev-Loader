-- Latest published version for fully automatic loader updates.
--
-- loader_config.latest_version is the newest build that exists. The client
-- sends its own version at startup and compares it with this value. When the
-- server is newer, loader_startup() also returns update_url and sha256, and the
-- loader downloads the exe, verifies the hash and restarts by itself before
-- the login form. min_version still acts as the hard cutoff that blocks old
-- clients; latest_version lets an allowed client update early.
--
-- Both loader_startup() and loader_bootstrap() now return the update fields on
-- success as well, so an update does not depend on the client being rejected.

alter table public.loader_config
	add column if not exists latest_version text not null default '0.1';

update public.loader_config
set latest_version = coalesce(nullif(latest_version, ''), '0.1')
where id = 1;

alter table public.loader_config
	drop constraint if exists loader_config_latest_version_format;

alter table public.loader_config
	add constraint loader_config_latest_version_format
	check (latest_version ~ '^\d{1,4}(\.\d{1,4}){0,3}$');

-- Public update fields for the client. Only reached through the security
-- definer functions below, so clients cannot select the config table directly.
create or replace function public.loader_update_info()
returns jsonb
language sql
stable
security definer
set search_path = public
as $$
	select jsonb_build_object(
		'latest_version', c.latest_version,
		'release_url', c.release_url,
		'update_url', c.update_url,
		'sha256', c.update_sha256
	)
	from public.loader_config c
	where c.id = 1;
$$;

revoke all on function public.loader_update_info() from public, anon, authenticated;

create or replace function public.loader_startup(p_hwid text, p_version text, p_exe_hash text)
returns jsonb
language plpgsql
volatile
security definer
set search_path = public
as $$
declare
	v_error jsonb;
	v_info jsonb;
begin
	v_error := public.loader_check_build(p_version, p_exe_hash);
	v_info := public.loader_update_info();

	insert into public.loader_check_log (kind, hwid, version, exe_hash, result)
	values ('startup', left(p_hwid, 80), left(p_version, 20), left(p_exe_hash, 64),
		coalesce(v_error ->> 'error', 'ok'));

	if v_error is not null then
		-- Maintenance is a temporary stop, not an update reason.
		if v_error ->> 'error' = 'maintenance' then
			return v_error;
		end if;
		return v_error || v_info;
	end if;

	return jsonb_build_object(
		'ok', true,
		'components', public.loader_component_list(false)
	) || v_info;
end;
$$;

create or replace function public.loader_bootstrap(p_hwid text, p_version text, p_exe_hash text)
returns jsonb
language plpgsql
volatile
security definer
set search_path = public
as $$
declare
	v_status jsonb;
	v_error jsonb;
	v_info jsonb;
begin
	if auth.uid() is null then
		return jsonb_build_object('ok', false, 'error', 'not_authenticated');
	end if;

	v_info := public.loader_update_info();

	-- Ban and HWID binding from the existing account function.
	v_status := to_jsonb(public.account_status(p_hwid));
	if coalesce((v_status ->> 'ok')::boolean, false) is not true then
		v_error := jsonb_build_object('ok', false, 'error', coalesce(v_status ->> 'error', 'not_authenticated'));
	else
		v_error := public.loader_check_build(p_version, p_exe_hash);
	end if;

	insert into public.loader_check_log (kind, user_id, hwid, version, exe_hash, result)
	values ('bootstrap', auth.uid(), left(p_hwid, 80), left(p_version, 20), left(p_exe_hash, 64),
		coalesce(v_error ->> 'error', 'ok'));

	if v_error is not null then
		if v_error ->> 'error' = 'maintenance' then
			return v_error;
		end if;
		return v_error || v_info;
	end if;

	return jsonb_build_object(
		'ok', true,
		'username', v_status ->> 'username',
		'components', public.loader_component_list(true)
	) || v_info;
end;
$$;

revoke all on function public.loader_startup(text, text, text) from public;
revoke all on function public.loader_bootstrap(text, text, text) from public, anon;
grant execute on function public.loader_startup(text, text, text) to anon, authenticated;
grant execute on function public.loader_bootstrap(text, text, text) to authenticated;
