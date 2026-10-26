-- Loader startup and post-login security checks.
--
-- loader_startup(p_hwid, p_version, p_exe_hash)
--   Called by the loader-auth Edge Function before the login form (anon role).
--   Rejects maintenance, outdated versions and unknown executables, then
--   returns the public component list.
--
-- loader_bootstrap(p_hwid, p_version, p_exe_hash)
--   Called right after login with the user token. Re-runs account_status()
--   (ban, HWID binding) and the same build checks, then returns every
--   enabled component.
--
-- All tables have RLS on and no policies: clients can only reach them through
-- these security definer functions.

create table if not exists public.loader_config (
	id int primary key default 1 check (id = 1),
	min_version text not null default '1.0.0',
	maintenance boolean not null default false,
	maintenance_message text not null default 'Loader is under maintenance. Try again later',
	-- Keep false while developing. Set true after adding release hashes to loader_builds.
	enforce_build_hash boolean not null default false
);

insert into public.loader_config (id) values (1) on conflict (id) do nothing;

create table if not exists public.loader_builds (
	exe_hash text primary key check (exe_hash ~ '^[0-9a-f]{64}$'),
	version text not null,
	active boolean not null default true,
	note text,
	created_at timestamptz not null default now()
);

create table if not exists public.loader_components (
	id bigint generated always as identity primary key,
	name text not null,
	version text not null default '1.0.0',
	description text not null default '',
	requires_login boolean not null default false,
	enabled boolean not null default true,
	sort int not null default 0
);

create table if not exists public.loader_check_log (
	id bigint generated always as identity primary key,
	created_at timestamptz not null default now(),
	kind text not null,
	user_id uuid,
	hwid text,
	version text,
	exe_hash text,
	result text not null
);

alter table public.loader_config enable row level security;
alter table public.loader_builds enable row level security;
alter table public.loader_components enable row level security;
alter table public.loader_check_log enable row level security;

revoke all on public.loader_config, public.loader_builds, public.loader_components, public.loader_check_log
	from anon, authenticated;

-- '1.2.10' -> {1,2,10}. Returns null for anything that is not a dotted number.
create or replace function public.loader_version_parts(p_version text)
returns int[]
language sql
immutable
as $$
	select case
		when p_version ~ '^\d{1,4}(\.\d{1,4}){0,3}$' then string_to_array(p_version, '.')::int[]
		else null
	end;
$$;

-- Shared build check. Returns null when the build is allowed, else an error object.
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
		return jsonb_build_object('ok', false, 'error', 'outdated');
	end if;

	if p_exe_hash is null or p_exe_hash !~ '^[0-9a-f]{64}$' then
		return jsonb_build_object('ok', false, 'error', 'bad_build');
	end if;

	if v_config.enforce_build_hash and not exists (
		select 1 from public.loader_builds b where b.exe_hash = p_exe_hash and b.active
	) then
		return jsonb_build_object('ok', false, 'error', 'bad_build');
	end if;

	return null;
end;
$$;

create or replace function public.loader_component_list(p_logged_in boolean)
returns jsonb
language sql
stable
security definer
set search_path = public
as $$
	select coalesce(jsonb_agg(jsonb_build_object(
		'name', c.name,
		'version', c.version,
		'description', c.description
	) order by c.sort, c.id), '[]'::jsonb)
	from public.loader_components c
	where c.enabled and (p_logged_in or not c.requires_login);
$$;

create or replace function public.loader_startup(p_hwid text, p_version text, p_exe_hash text)
returns jsonb
language plpgsql
volatile
security definer
set search_path = public
as $$
declare
	v_error jsonb;
begin
	v_error := public.loader_check_build(p_version, p_exe_hash);

	insert into public.loader_check_log (kind, hwid, version, exe_hash, result)
	values ('startup', left(p_hwid, 80), left(p_version, 20), left(p_exe_hash, 64),
		coalesce(v_error ->> 'error', 'ok'));

	if v_error is not null then
		return v_error;
	end if;

	return jsonb_build_object('ok', true, 'components', public.loader_component_list(false));
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
begin
	if auth.uid() is null then
		return jsonb_build_object('ok', false, 'error', 'not_authenticated');
	end if;

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
		return v_error;
	end if;

	return jsonb_build_object(
		'ok', true,
		'username', v_status ->> 'username',
		'components', public.loader_component_list(true)
	);
end;
$$;

revoke all on function public.loader_check_build(text, text) from public, anon, authenticated;
revoke all on function public.loader_component_list(boolean) from public, anon, authenticated;
revoke all on function public.loader_startup(text, text, text) from public;
revoke all on function public.loader_bootstrap(text, text, text) from public, anon;

grant execute on function public.loader_startup(text, text, text) to anon, authenticated;
grant execute on function public.loader_bootstrap(text, text, text) to authenticated;
