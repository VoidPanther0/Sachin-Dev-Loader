-- Latest release page for clients that are rejected by the build checks.
--
-- loader_startup() and loader_bootstrap() now return the release URL together
-- with the "outdated" or "bad_build" error. The loader-auth function forwards
-- it as "download_url", and the client shows a Download button that opens it.
--
-- Old clients keep the generic message, but every client from 0.1 onward can
-- send users straight to the current GitHub release.

alter table public.loader_config
	add column if not exists release_url text not null
	default 'https://github.com/sapkotasachin064-beep/Sachin-Dev-Loader/releases/latest';

update public.loader_config
set min_version = '0.1',
	release_url = 'https://github.com/sapkotasachin064-beep/Sachin-Dev-Loader/releases/latest'
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
		return jsonb_build_object('ok', false, 'error', 'outdated', 'url', v_config.release_url);
	end if;

	if p_exe_hash is null or p_exe_hash !~ '^[0-9a-f]{64}$' then
		return jsonb_build_object('ok', false, 'error', 'bad_build', 'url', v_config.release_url);
	end if;

	if v_config.enforce_build_hash and not exists (
		select 1 from public.loader_builds b where b.exe_hash = p_exe_hash and b.active
	) then
		return jsonb_build_object('ok', false, 'error', 'bad_build', 'url', v_config.release_url);
	end if;

	return null;
end;
$$;
