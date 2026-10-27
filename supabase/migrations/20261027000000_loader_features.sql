-- Sachin Dev Loader: v0.4.0 features.
--
--   * loader_components: download_url / sha256 (launch from the client) and a
--     status label (online, updating, offline, beta)
--   * loader_news: release notes shown on the News page
--   * loader_config: announcement banner and support link
--   * profiles.expires_at: per-account access expiry (null = lifetime)
--   * admin_* helpers for everything above
--
-- Apply after 20261026120000_loader_cleanup.sql:
--   supabase db push
--   supabase functions deploy loader-auth --no-verify-jwt

-- ---------------------------------------------------------------- columns ----

alter table public.profiles add column if not exists expires_at timestamptz;

alter table public.loader_config add column if not exists announcement text not null default '';
alter table public.loader_config add column if not exists announcement_level text not null default 'info';
alter table public.loader_config add column if not exists support_url text not null default '';

alter table public.loader_components add column if not exists download_url text not null default '';
alter table public.loader_components add column if not exists sha256 text not null default '';
alter table public.loader_components add column if not exists status text not null default 'online';

do $$
begin
  if not exists (select 1 from pg_constraint where conname = 'loader_config_announcement_level'
                 and conrelid = 'public.loader_config'::regclass) then
    alter table public.loader_config
      add constraint loader_config_announcement_level check (announcement_level in ('info', 'warning'));
  end if;

  if not exists (select 1 from pg_constraint where conname = 'loader_components_status'
                 and conrelid = 'public.loader_components'::regclass) then
    alter table public.loader_components
      add constraint loader_components_status check (status in ('online', 'updating', 'offline', 'beta'));
  end if;

  if not exists (select 1 from pg_constraint where conname = 'loader_components_download'
                 and conrelid = 'public.loader_components'::regclass) then
    alter table public.loader_components
      add constraint loader_components_download check (
        download_url = '' or (download_url ~ '^https://' and sha256 ~ '^[0-9a-f]{64}$')
      );
  end if;
end;
$$;

-- ------------------------------------------------------------------- news ----

create table if not exists public.loader_news (
  id bigint generated always as identity primary key,
  version text not null default '',
  published_on date not null default current_date,
  tag text not null default 'UPDATE' check (tag in ('FEATURE', 'FIX', 'UPDATE', 'RELEASE', 'NOTICE')),
  title text not null,
  body text not null default '',
  enabled boolean not null default true,
  created_at timestamptz not null default now()
);

insert into public.loader_news (version, published_on, tag, title, body)
select v.version, v.published_on::date, v.tag, v.title, v.body
from (values
  ('0.3.2', '2026-10-27', 'UPDATE',  'Cleaner releases',
   'Release builds no longer publish to Supabase from CI. The database schema was rebuilt into one migration.'),
  ('0.3.1', '2026-10-26', 'FIX',     'Legacy update helper',
   'Clients still on 0.1 keep a legacy helper, so they can update to the new in-place swap.'),
  ('0.3.0', '2026-10-26', 'FEATURE', 'Fully automatic updates',
   'The startup check downloads the new build, verifies its SHA-256, replaces the exe and restarts.'),
  ('0.1',   '2026-10-26', 'RELEASE', 'First release',
   'Supabase login with hardware binding, server build checks and a component list.')
) as v(version, published_on, tag, title, body)
where not exists (select 1 from public.loader_news);

-- ---------------------------------------------------------------- helpers ----

create or replace function public.loader_news_json()
returns jsonb
language sql
stable
security definer
set search_path = ''
as $$
  select coalesce(jsonb_agg(
    jsonb_build_object(
      'version', n.version,
      'date', to_char(n.published_on, 'YYYY-MM-DD'),
      'tag', n.tag,
      'title', n.title,
      'body', n.body
    )
    order by n.published_on desc, n.id desc
  ), '[]'::jsonb)
  from (
    select * from public.loader_news
    where enabled
    order by published_on desc, id desc
    limit 20
  ) n;
$$;

-- Announcement, support link and news: public, sent with startup and bootstrap.
create or replace function public.loader_public_info()
returns jsonb
language sql
stable
security definer
set search_path = ''
as $$
  select jsonb_build_object(
    'announcement', jsonb_build_object('message', c.announcement, 'level', c.announcement_level),
    'support_url', c.support_url,
    'news', public.loader_news_json()
  )
  from public.loader_config c
  where c.id = 1;
$$;

-- Download data is only released to signed-in accounts.
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
        'description', c.description,
        'status', c.status,
        'download_url', case when p_authenticated then c.download_url else '' end,
        'sha256', case when p_authenticated then c.sha256 else '' end
      )
      order by c.sort_order, c.name
    ),
    '[]'::jsonb
  )
  from public.loader_components c
  where c.enabled
    and (p_authenticated or not c.requires_login);
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

  if v_profile.expires_at is not null and v_profile.expires_at <= now() then
    return jsonb_build_object('ok', false, 'error', 'expired');
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

  if v_profile.expires_at is not null and v_profile.expires_at <= now() then
    return jsonb_build_object('ok', false, 'error', 'expired');
  end if;

  if v_profile.hwid is not null and upper(v_profile.hwid) <> v_hwid then
    return jsonb_build_object('ok', false, 'error', 'hwid_mismatch');
  end if;

  return jsonb_build_object(
    'ok', true,
    'username', v_profile.username,
    'hwid', v_profile.hwid,
    'expires_at', extract(epoch from v_profile.expires_at)::bigint
  );
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
  ) || v_info || public.loader_public_info();
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
    'expires_at', coalesce((v_status ->> 'expires_at')::bigint, 0),
    'components', public.loader_components_json(true)
  ) || v_info || public.loader_public_info();
end;
$$;

-- ------------------------------------------------------------------ admin ----

-- Days from now; null clears the expiry (lifetime). p_days = 0 expires at once.
create or replace function public.admin_set_expiry(p_username text, p_days integer)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_count integer;
begin
  update public.profiles
  set expires_at = case when p_days is null then null else now() + make_interval(days => p_days) end,
      updated_at = now()
  where username = lower(trim(coalesce(p_username, '')));
  get diagnostics v_count = row_count;

  return jsonb_build_object('ok', v_count > 0, 'updated', v_count);
end;
$$;

-- Adds days to the current expiry (or to now when expired or lifetime-less).
create or replace function public.admin_extend_expiry(p_username text, p_days integer)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_count integer;
begin
  update public.profiles
  set expires_at = greatest(coalesce(expires_at, now()), now()) + make_interval(days => p_days),
      updated_at = now()
  where username = lower(trim(coalesce(p_username, '')));
  get diagnostics v_count = row_count;

  return jsonb_build_object('ok', v_count > 0, 'updated', v_count);
end;
$$;

create or replace function public.admin_set_announcement(p_message text, p_level text default 'info')
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
begin
  update public.loader_config
  set announcement = left(trim(coalesce(p_message, '')), 240),
      announcement_level = case when p_level = 'warning' then 'warning' else 'info' end,
      updated_at = now()
  where id = 1;

  return jsonb_build_object('ok', true);
end;
$$;

create or replace function public.admin_set_support_url(p_url text)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
begin
  if coalesce(p_url, '') <> '' and p_url !~ '^https://' then
    return jsonb_build_object('ok', false, 'error', 'invalid_url');
  end if;

  update public.loader_config
  set support_url = coalesce(p_url, ''),
      updated_at = now()
  where id = 1;

  return jsonb_build_object('ok', true);
end;
$$;

-- Creates or updates a component. Pass an empty download url for a component
-- that is listed but cannot be launched yet.
create or replace function public.admin_upsert_component(
  p_name text,
  p_version text,
  p_description text default '',
  p_download_url text default '',
  p_sha256 text default '',
  p_status text default 'online',
  p_requires_login boolean default true,
  p_sort_order integer default 100
)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_name text := trim(coalesce(p_name, ''));
  v_sha text := lower(trim(coalesce(p_sha256, '')));
  v_url text := trim(coalesce(p_download_url, ''));
begin
  if v_name = '' or length(v_name) > 40 then
    return jsonb_build_object('ok', false, 'error', 'invalid_name');
  end if;
  if p_status not in ('online', 'updating', 'offline', 'beta') then
    return jsonb_build_object('ok', false, 'error', 'invalid_status');
  end if;
  if v_url <> '' and (v_url !~ '^https://' or v_sha !~ '^[0-9a-f]{64}$') then
    return jsonb_build_object('ok', false, 'error', 'invalid_download');
  end if;

  insert into public.loader_components (
    name, version, description, download_url, sha256, status, requires_login, enabled, sort_order
  ) values (
    v_name, trim(coalesce(p_version, '')), coalesce(p_description, ''), v_url, case when v_url = '' then '' else v_sha end,
    p_status, coalesce(p_requires_login, true), true, coalesce(p_sort_order, 100)
  )
  on conflict (name) do update set
    version = excluded.version,
    description = excluded.description,
    download_url = excluded.download_url,
    sha256 = excluded.sha256,
    status = excluded.status,
    requires_login = excluded.requires_login,
    enabled = true,
    sort_order = excluded.sort_order,
    updated_at = now();

  return jsonb_build_object('ok', true, 'name', v_name);
end;
$$;

create or replace function public.admin_set_component_status(p_name text, p_status text)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_count integer;
begin
  if p_status not in ('online', 'updating', 'offline', 'beta') then
    return jsonb_build_object('ok', false, 'error', 'invalid_status');
  end if;

  update public.loader_components
  set status = p_status,
      updated_at = now()
  where name = trim(coalesce(p_name, ''));
  get diagnostics v_count = row_count;

  return jsonb_build_object('ok', v_count > 0, 'updated', v_count);
end;
$$;

create or replace function public.admin_remove_component(p_name text)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_count integer;
begin
  delete from public.loader_components where name = trim(coalesce(p_name, ''));
  get diagnostics v_count = row_count;

  return jsonb_build_object('ok', v_count > 0, 'deleted', v_count);
end;
$$;

create or replace function public.admin_publish_news(
  p_title text,
  p_body text default '',
  p_tag text default 'UPDATE',
  p_version text default ''
)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_id bigint;
begin
  if trim(coalesce(p_title, '')) = '' then
    return jsonb_build_object('ok', false, 'error', 'invalid_title');
  end if;
  if upper(coalesce(p_tag, '')) not in ('FEATURE', 'FIX', 'UPDATE', 'RELEASE', 'NOTICE') then
    return jsonb_build_object('ok', false, 'error', 'invalid_tag');
  end if;

  insert into public.loader_news (version, tag, title, body)
  values (trim(coalesce(p_version, '')), upper(p_tag), left(trim(p_title), 80), left(coalesce(p_body, ''), 600))
  returning id into v_id;

  return jsonb_build_object('ok', true, 'id', v_id);
end;
$$;

create or replace function public.admin_remove_news(p_id bigint)
returns jsonb
language plpgsql
security definer
set search_path = ''
as $$
declare
  v_count integer;
begin
  delete from public.loader_news where id = p_id;
  get diagnostics v_count = row_count;

  return jsonb_build_object('ok', v_count > 0, 'deleted', v_count);
end;
$$;

create or replace function public.admin_list_news()
returns table (id bigint, version text, published_on date, tag text, title text, enabled boolean)
language sql
security definer
set search_path = ''
as $$
  select n.id, n.version, n.published_on, n.tag, n.title, n.enabled
  from public.loader_news n
  order by n.published_on desc, n.id desc;
$$;

create or replace function public.admin_list_components()
returns table (name text, version text, status text, launchable boolean, enabled boolean, requires_login boolean)
language sql
security definer
set search_path = ''
as $$
  select c.name, c.version, c.status, c.download_url <> '', c.enabled, c.requires_login
  from public.loader_components c
  order by c.sort_order, c.name;
$$;

-- Replaces the list function so it also reports each account's expiry.
drop function if exists public.admin_list_accounts();

create function public.admin_list_accounts()
returns table (
  username text,
  is_banned boolean,
  created_at timestamptz,
  last_login_at timestamptz,
  bound_hwid text,
  login_count integer,
  expires_at timestamptz
)
language sql
security definer
set search_path = ''
as $$
  select p.username, p.is_banned, p.created_at, p.last_login_at, p.hwid, p.login_count, p.expires_at
  from public.profiles p
  order by p.created_at desc;
$$;

-- ---------------------------------------------------------------- grants ----

revoke all on public.loader_news from anon, authenticated;
grant all on public.loader_news to service_role;
grant usage, select on all sequences in schema public to service_role;
alter table public.loader_news enable row level security;

revoke all on function public.loader_news_json() from public, anon, authenticated;
revoke all on function public.loader_public_info() from public, anon, authenticated;
revoke all on function public.account_login(text) from public, anon, authenticated;
revoke all on function public.account_status(text) from public, anon, authenticated;
revoke all on function public.loader_startup(text, text, text) from public, anon, authenticated;
revoke all on function public.loader_bootstrap(text, text, text) from public, anon, authenticated;
revoke all on function public.loader_components_json(boolean) from public, anon, authenticated;
revoke all on function public.admin_set_expiry(text, integer) from public, anon, authenticated;
revoke all on function public.admin_extend_expiry(text, integer) from public, anon, authenticated;
revoke all on function public.admin_set_announcement(text, text) from public, anon, authenticated;
revoke all on function public.admin_set_support_url(text) from public, anon, authenticated;
revoke all on function public.admin_upsert_component(text, text, text, text, text, text, boolean, integer) from public, anon, authenticated;
revoke all on function public.admin_set_component_status(text, text) from public, anon, authenticated;
revoke all on function public.admin_remove_component(text) from public, anon, authenticated;
revoke all on function public.admin_publish_news(text, text, text, text) from public, anon, authenticated;
revoke all on function public.admin_remove_news(bigint) from public, anon, authenticated;
revoke all on function public.admin_list_news() from public, anon, authenticated;
revoke all on function public.admin_list_components() from public, anon, authenticated;
revoke all on function public.admin_list_accounts() from public, anon, authenticated;

grant execute on function public.loader_news_json() to service_role;
grant execute on function public.loader_public_info() to service_role;
grant execute on function public.loader_components_json(boolean) to service_role;
grant execute on function public.loader_startup(text, text, text) to anon, authenticated, service_role;
grant execute on function public.loader_bootstrap(text, text, text) to authenticated, service_role;
grant execute on function public.account_login(text) to authenticated, service_role;
grant execute on function public.account_status(text) to authenticated, service_role;
grant execute on function public.admin_set_expiry(text, integer) to service_role;
grant execute on function public.admin_extend_expiry(text, integer) to service_role;
grant execute on function public.admin_set_announcement(text, text) to service_role;
grant execute on function public.admin_set_support_url(text) to service_role;
grant execute on function public.admin_upsert_component(text, text, text, text, text, text, boolean, integer) to service_role;
grant execute on function public.admin_set_component_status(text, text) to service_role;
grant execute on function public.admin_remove_component(text) to service_role;
grant execute on function public.admin_publish_news(text, text, text, text) to service_role;
grant execute on function public.admin_remove_news(bigint) to service_role;
grant execute on function public.admin_list_news() to service_role;
grant execute on function public.admin_list_components() to service_role;
grant execute on function public.admin_list_accounts() to service_role;
