-- Sachin Dev Loader: clean schema for v0.3.2.
--
-- The public schema is rebuilt around three tables:
--   profiles           one row per loader account (bound to auth.users)
--   loader_config      one row with the current release and maintenance state
--   loader_components  content released to the client
--
-- The Edge Function loader-auth is the only caller. Direct table access is
-- revoked from anon and authenticated; every read and write goes through a
-- SECURITY DEFINER function with a fixed empty search_path.

create extension if not exists pgcrypto with schema extensions;

-- ---------------------------------------------------------------- tables ----

create table if not exists public.profiles (
  id uuid primary key references auth.users(id) on delete cascade,
  username text not null unique,
  is_banned boolean not null default false,
  hwid text,
  hwid_bound_at timestamptz,
  last_login_at timestamptz,
  login_count integer not null default 0,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

alter table public.profiles add column if not exists updated_at timestamptz not null default now();

do $$
begin
  if not exists (
    select 1 from pg_constraint
    where conname = 'profiles_username_format'
      and conrelid = 'public.profiles'::regclass
  ) then
    alter table public.profiles
      add constraint profiles_username_format check (username ~ '^[a-z0-9_]{2,20}$');
  end if;
end;
$$;

create table if not exists public.loader_config (
  id smallint primary key default 1,
  maintenance boolean not null default false,
  maintenance_message text not null default '',
  min_version text not null default '0.3.0',
  latest_version text not null default '0.3.1',
  update_url text not null default '',
  update_sha256 text not null default '',
  release_url text not null default '',
  auto_sync boolean not null default true,
  release_checked_at timestamptz,
  updated_at timestamptz not null default now(),
  constraint loader_config_single_row check (id = 1)
);

alter table public.loader_config add column if not exists auto_sync boolean not null default true;
alter table public.loader_config add column if not exists release_checked_at timestamptz;
alter table public.loader_config add column if not exists updated_at timestamptz not null default now();

create table if not exists public.loader_components (
  id bigint generated always as identity primary key,
  name text not null unique,
  version text not null,
  description text not null default '',
  requires_login boolean not null default false,
  enabled boolean not null default true,
  sort_order integer not null default 0,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

alter table public.loader_components add column if not exists enabled boolean not null default true;
alter table public.loader_components add column if not exists sort_order integer not null default 0;
alter table public.loader_components add column if not exists updated_at timestamptz not null default now();

-- ------------------------------------------------------------- seed data ----

-- Keep the last known good release. The Edge Function replaces this with the
-- newest GitHub release as soon as it is published.
insert into public.loader_config (
  id, maintenance, maintenance_message, min_version, latest_version,
  update_url, update_sha256, release_url, auto_sync
) values (
  1, false, '', '0.3.0', '0.3.1',
  'https://github.com/sapkotasachin064-beep/Sachin-Dev-Loader/releases/download/v0.3.1/ImGuiTemplate.exe',
  '4c626a4b5fe8b875e8a2bf6fec91842b6b1558c932e2aede84560dffaeafe810',
  'https://github.com/sapkotasachin064-beep/Sachin-Dev-Loader/releases/tag/v0.3.1',
  true
)
on conflict (id) do update set
  maintenance = excluded.maintenance,
  maintenance_message = excluded.maintenance_message,
  min_version = excluded.min_version,
  latest_version = excluded.latest_version,
  update_url = excluded.update_url,
  update_sha256 = excluded.update_sha256,
  release_url = excluded.release_url,
  auto_sync = excluded.auto_sync,
  updated_at = now();

-- Rebuild profile rows for the existing loader accounts.
insert into public.profiles (id, username)
select u.id, split_part(lower(u.email), '@', 1)
from auth.users u
where lower(u.email) like '%@loader.local'
  and split_part(lower(u.email), '@', 1) ~ '^[a-z0-9_]{2,20}$'
on conflict (id) do update set
  username = excluded.username,
  updated_at = now();

-- Requested accounts and passwords. HWIDs are cleared so each account can bind
-- to its current machine on the next login.
update public.profiles
set hwid = null,
    hwid_bound_at = null,
    is_banned = false,
    updated_at = now()
where username in ('111', 'fz');

update auth.users
set encrypted_password = extensions.crypt('111', extensions.gen_salt('bf', 10)),
    email_confirmed_at = coalesce(email_confirmed_at, now()),
    updated_at = now()
where lower(email) = '111@loader.local';

update auth.users
set encrypted_password = extensions.crypt('fz', extensions.gen_salt('bf', 10)),
    email_confirmed_at = coalesce(email_confirmed_at, now()),
    updated_at = now()
where lower(email) = 'fz@loader.local';

insert into public.loader_components (name, version, description, requires_login, enabled, sort_order)
values ('Core', '0.3.2', 'Loader core and session tools', true, true, 10)
on conflict (name) do update set
  version = excluded.version,
  description = excluded.description,
  requires_login = excluded.requires_login,
  enabled = excluded.enabled,
  sort_order = excluded.sort_order,
  updated_at = now();

-- -------------------------------------------------------------- helpers ----

create or replace function public.loader_version_parts(p_version text)
returns integer[]
language sql
immutable
set search_path = ''
as $$
  select case
    when coalesce(p_version, '') ~ '^[0-9]{1,4}([.][0-9]{1,4}){0,3}$'
      then string_to_array(p_version, '.')::integer[]
    else null
  end;
$$;

create or replace function public.loader_version_cmp(p_left text, p_right text)
returns integer
language plpgsql
immutable
set search_path = ''
as $$
declare
  v_left integer[] := public.loader_version_parts(p_left);
  v_right integer[] := public.loader_version_parts(p_right);
  i integer;
  a integer;
  b integer;
begin
  if v_left is null or v_right is null then
    return null;
  end if;

  for i in 1..4 loop
    a := coalesce(v_left[i], 0);
    b := coalesce(v_right[i], 0);
    if a < b then return -1; end if;
    if a > b then return 1; end if;
  end loop;

  return 0;
end;
$$;

create or replace function public.loader_update_info()
returns jsonb
language sql
stable
security definer
set search_path = ''
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

create or replace function public.loader_components_json(p_authenticated boolean)
returns jsonb
language sql
stable
security definer
set search_path = ''
as $$
  select coalesce(
    jsonb_agg(
      jsonb_build_object(
        'name', c.name,
        'version', c.version,
        'description', c.description
      )
      order by c.sort_order, c.name
    ),
    '[]'::jsonb
  )
  from public.loader_components c
  where c.enabled
    and (p_authenticated or not c.requires_login);
$$;

-- ---------------------------------------------------------- build checks ----

create or replace function public.loader_check_build(p_version text, p_exe_hash text)
returns jsonb
language plpgsql
stable
security definer
set search_path = ''
as $$
declare
  v_config public.loader_config%rowtype;
  v_cmp integer;
begin
  select * into v_config from public.loader_config where id = 1;
  if not found then
    return jsonb_build_object(
      'ok', false,
      'error', 'config_missing',
      'message', 'Loader configuration is missing'
    );
  end if;

  if v_config.maintenance then
    return jsonb_build_object(
      'ok', false,
      'error', 'maintenance',
      'message', coalesce(nullif(v_config.maintenance_message, ''), 'Loader is under maintenance. Try again later')
    );
  end if;

  if public.loader_version_parts(p_version) is null then
    return jsonb_build_object(
      'ok', false,
      'error', 'bad_build',
      'message', 'Loader version is not valid'
    );
  end if;

  v_cmp := public.loader_version_cmp(p_version, v_config.min_version);
  if v_cmp is null or v_cmp < 0 then
    return jsonb_build_object(
      'ok', false,
      'error', 'outdated',
      'message', 'This loader version is outdated. Download the latest build',
      'url', v_config.release_url,
      'release_url', v_config.release_url,
      'update_url', v_config.update_url,
      'sha256', v_config.update_sha256,
      'latest_version', v_config.latest_version
    );
  end if;

  if p_exe_hash is null or p_exe_hash !~ '^[0-9a-f]{64}$' then
    return jsonb_build_object(
      'ok', false,
      'error', 'bad_build',
      'message', 'Loader file is modified or unknown. Download the official build',
      'url', v_config.release_url,
      'release_url', v_config.release_url,
      'update_url', v_config.update_url,
      'sha256', v_config.update_sha256,
      'latest_version', v_config.latest_version
    );
  end if;

  return null;
end;
$$;

-- --------------------------------------------------------------- account ----

create or replace function public.account_login(p_hwid text)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_uid uuid := auth.uid();
  v_hwid text := upper(trim(coalesce(p_hwid, '')));
  v_profile public.profiles%rowtype;
begin
  if v_uid is null then
    return jsonb_build_object('ok', false, 'error', 'not_authenticated');
  end if;

  select * into v_profile from public.profiles p where p.id = v_uid;
  if not found then
    return jsonb_build_object('ok', false, 'error', 'no_account');
  end if;

  if v_profile.is_banned then
    return jsonb_build_object('ok', false, 'error', 'banned');
  end if;

  if v_hwid = '' then
    return jsonb_build_object('ok', false, 'error', 'invalid_input');
  end if;

  if v_profile.hwid is null then
    update public.profiles
    set hwid = v_hwid,
        hwid_bound_at = now(),
        last_login_at = now(),
        login_count = login_count + 1,
        updated_at = now()
    where id = v_uid;
  elsif upper(v_profile.hwid) <> v_hwid then
    return jsonb_build_object('ok', false, 'error', 'hwid_mismatch');
  else
    update public.profiles
    set last_login_at = now(),
        login_count = login_count + 1,
        updated_at = now()
    where id = v_uid;
  end if;

  return jsonb_build_object('ok', true, 'username', v_profile.username, 'hwid', v_hwid);
end;
$$;

create or replace function public.account_status(p_hwid text)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_uid uuid := auth.uid();
  v_hwid text := upper(trim(coalesce(p_hwid, '')));
  v_profile public.profiles%rowtype;
begin
  if v_uid is null then
    return jsonb_build_object('ok', false, 'error', 'not_authenticated');
  end if;

  select * into v_profile from public.profiles p where p.id = v_uid;
  if not found then
    return jsonb_build_object('ok', false, 'error', 'no_account');
  end if;

  if v_profile.is_banned then
    return jsonb_build_object('ok', false, 'error', 'banned');
  end if;

  if v_profile.hwid is not null and upper(v_profile.hwid) <> v_hwid then
    return jsonb_build_object('ok', false, 'error', 'hwid_mismatch');
  end if;

  return jsonb_build_object('ok', true, 'username', v_profile.username, 'hwid', v_profile.hwid);
end;
$$;

-- ------------------------------------------------------------- bootstrap ----

create or replace function public.loader_startup(p_hwid text, p_version text, p_exe_hash text)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_error jsonb;
  v_info jsonb;
begin
  v_error := public.loader_check_build(p_version, p_exe_hash);
  v_info := public.loader_update_info();

  if v_error is not null then
    if v_error ->> 'error' = 'maintenance' then
      return v_error;
    end if;
    return v_error || v_info;
  end if;

  return jsonb_build_object(
    'ok', true,
    'components', public.loader_components_json(false)
  ) || v_info;
end;
$$;

create or replace function public.loader_bootstrap(p_hwid text, p_version text, p_exe_hash text)
returns jsonb
language plpgsql
security definer
set search_path = ''
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
  v_status := public.account_status(p_hwid);

  if coalesce((v_status ->> 'ok')::boolean, false) is not true then
    v_error := jsonb_build_object('ok', false, 'error', coalesce(v_status ->> 'error', 'not_authenticated'));
  else
    v_error := public.loader_check_build(p_version, p_exe_hash);
  end if;

  if v_error is not null then
    if v_error ->> 'error' = 'maintenance' then
      return v_error;
    end if;
    return v_error || v_info;
  end if;

  return jsonb_build_object(
    'ok', true,
    'username', v_status ->> 'username',
    'components', public.loader_components_json(true)
  ) || v_info;
end;
$$;

-- ---------------------------------------------------------------- release ----

create or replace function public.loader_sync_begin()
returns boolean
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_due boolean;
begin
  update public.loader_config
  set release_checked_at = now(),
      updated_at = now()
  where id = 1
    and auto_sync
    and (release_checked_at is null or release_checked_at < now() - interval '5 minutes')
  returning true into v_due;

  return coalesce(v_due, false);
end;
$$;

create or replace function public.loader_sync_release(
  p_version text,
  p_update_url text,
  p_sha256 text,
  p_release_url text
)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_version text := trim(coalesce(p_version, ''));
  v_current text;
begin
  if public.loader_version_parts(v_version) is null then
    return jsonb_build_object('ok', false, 'error', 'invalid_version');
  end if;

  if coalesce(p_sha256, '') !~ '^[0-9a-fA-F]{64}$' then
    return jsonb_build_object('ok', false, 'error', 'invalid_sha256');
  end if;

  if coalesce(p_update_url, '') !~ '^https://' then
    return jsonb_build_object('ok', false, 'error', 'invalid_update_url');
  end if;

  select latest_version into v_current from public.loader_config where id = 1;
  if v_current is not null and public.loader_version_cmp(v_version, v_current) < 0 then
    return jsonb_build_object('ok', false, 'error', 'older_release', 'current', v_current);
  end if;

  update public.loader_config
  set latest_version = v_version,
      update_url = p_update_url,
      update_sha256 = lower(p_sha256),
      release_url = coalesce(p_release_url, ''),
      release_checked_at = now(),
      updated_at = now()
  where id = 1;

  return jsonb_build_object('ok', true, 'version', v_version);
end;
$$;

-- ------------------------------------------------------------------ admin ----

create or replace function public.admin_create_account(p_username text, p_password text)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_username text := lower(trim(coalesce(p_username, '')));
  v_email text;
  v_uid uuid := gen_random_uuid();
begin
  if v_username !~ '^[a-z0-9_]{2,20}$' then
    return jsonb_build_object('ok', false, 'error', 'invalid_username');
  end if;

  if length(coalesce(p_password, '')) < 2 then
    return jsonb_build_object('ok', false, 'error', 'weak_password');
  end if;

  v_email := v_username || '@loader.local';

  if exists (select 1 from auth.users u where lower(u.email) = v_email) then
    return jsonb_build_object('ok', false, 'error', 'username_taken');
  end if;

  insert into auth.users (
    instance_id, id, aud, role, email, encrypted_password, email_confirmed_at,
    confirmation_token, recovery_token, email_change, email_change_token_new, email_change_token_current,
    phone_change, phone_change_token, reauthentication_token,
    raw_app_meta_data, raw_user_meta_data, created_at, updated_at
  ) values (
    '00000000-0000-0000-0000-000000000000', v_uid, 'authenticated', 'authenticated', v_email,
    extensions.crypt(p_password, extensions.gen_salt('bf', 10)), now(),
    '', '', '', '', '',
    '', '', '',
    '{"provider":"email","providers":["email"]}'::jsonb,
    jsonb_build_object('username', v_username, 'email_verified', true, 'phone_verified', false),
    now(), now()
  );

  insert into auth.identities (id, user_id, provider_id, identity_data, provider, last_sign_in_at, created_at, updated_at)
  values (
    gen_random_uuid(), v_uid, v_uid::text,
    jsonb_build_object('sub', v_uid::text, 'email', v_email, 'email_verified', true, 'phone_verified', false),
    'email', now(), now(), now()
  );

  insert into public.profiles (id, username) values (v_uid, v_username);

  return jsonb_build_object('ok', true, 'username', v_username, 'email', v_email, 'user_id', v_uid);
exception when unique_violation then
  return jsonb_build_object('ok', false, 'error', 'conflict');
end;
$$;

create or replace function public.admin_set_password(p_username text, p_password text)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_email text := lower(trim(coalesce(p_username, ''))) || '@loader.local';
  v_count integer;
begin
  if length(coalesce(p_password, '')) < 2 then
    return jsonb_build_object('ok', false, 'error', 'weak_password');
  end if;

  update auth.users
  set encrypted_password = extensions.crypt(p_password, extensions.gen_salt('bf', 10)),
      updated_at = now()
  where lower(email) = v_email;
  get diagnostics v_count = row_count;

  return jsonb_build_object('ok', v_count > 0, 'updated', v_count);
end;
$$;

create or replace function public.admin_reset_hwid(p_username text)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_count integer;
begin
  update public.profiles
  set hwid = null,
      hwid_bound_at = null,
      updated_at = now()
  where username = lower(trim(coalesce(p_username, '')));
  get diagnostics v_count = row_count;

  return jsonb_build_object('ok', v_count > 0, 'updated', v_count);
end;
$$;

create or replace function public.admin_set_banned(p_username text, p_banned boolean)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_count integer;
begin
  update public.profiles
  set is_banned = coalesce(p_banned, false),
      updated_at = now()
  where username = lower(trim(coalesce(p_username, '')));
  get diagnostics v_count = row_count;

  return jsonb_build_object('ok', v_count > 0, 'updated', v_count);
end;
$$;

create or replace function public.admin_list_accounts()
returns table (
  username text,
  is_banned boolean,
  created_at timestamptz,
  last_login_at timestamptz,
  bound_hwid text,
  login_count integer
)
language sql
security definer
set search_path = ''
as $$
  select p.username, p.is_banned, p.created_at, p.last_login_at, p.hwid, p.login_count
  from public.profiles p
  order by p.created_at desc;
$$;

-- --------------------------------------------------------------- cleanup ----

drop function if exists public.loader_client_ip();
drop table if exists public.loader_builds cascade;
drop table if exists public.loader_check_log cascade;

-- ---------------------------------------------------------------- grants ----

revoke all on public.profiles from anon, authenticated;
revoke all on public.loader_config from anon, authenticated;
revoke all on public.loader_components from anon, authenticated;
grant all on public.profiles to service_role;
grant all on public.loader_config to service_role;
grant all on public.loader_components to service_role;
grant usage, select on all sequences in schema public to service_role;

revoke all on function public.loader_version_parts(text) from public, anon, authenticated;
revoke all on function public.loader_version_cmp(text, text) from public, anon, authenticated;
revoke all on function public.loader_update_info() from public, anon, authenticated;
revoke all on function public.loader_components_json(boolean) from public, anon, authenticated;
revoke all on function public.loader_check_build(text, text) from public, anon, authenticated;
revoke all on function public.loader_startup(text, text, text) from public, anon, authenticated;
revoke all on function public.loader_bootstrap(text, text, text) from public, anon, authenticated;
revoke all on function public.account_login(text) from public, anon, authenticated;
revoke all on function public.account_status(text) from public, anon, authenticated;
revoke all on function public.loader_sync_begin() from public, anon, authenticated;
revoke all on function public.loader_sync_release(text, text, text, text) from public, anon, authenticated;
revoke all on function public.admin_create_account(text, text) from public, anon, authenticated;
revoke all on function public.admin_set_password(text, text) from public, anon, authenticated;
revoke all on function public.admin_reset_hwid(text) from public, anon, authenticated;
revoke all on function public.admin_set_banned(text, boolean) from public, anon, authenticated;
revoke all on function public.admin_list_accounts() from public, anon, authenticated;

grant execute on function public.loader_startup(text, text, text) to anon, authenticated, service_role;
grant execute on function public.loader_bootstrap(text, text, text) to authenticated, service_role;
grant execute on function public.account_login(text) to authenticated, service_role;
grant execute on function public.account_status(text) to authenticated, service_role;
grant execute on function public.loader_sync_begin() to service_role;
grant execute on function public.loader_sync_release(text, text, text, text) to service_role;
grant execute on function public.admin_create_account(text, text) to service_role;
grant execute on function public.admin_set_password(text, text) to service_role;
grant execute on function public.admin_reset_hwid(text) to service_role;
grant execute on function public.admin_set_banned(text, boolean) to service_role;
grant execute on function public.admin_list_accounts() to service_role;
grant execute on function public.loader_version_parts(text) to service_role;
grant execute on function public.loader_version_cmp(text, text) to service_role;
grant execute on function public.loader_update_info() to service_role;
grant execute on function public.loader_components_json(boolean) to service_role;
grant execute on function public.loader_check_build(text, text) to service_role;

-- --------------------------------------------------------------- rls ------

alter table public.profiles enable row level security;
alter table public.loader_config enable row level security;
alter table public.loader_components enable row level security;
