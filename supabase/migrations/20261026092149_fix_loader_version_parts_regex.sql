-- Fix the version parser used by loader_startup() and loader_bootstrap().
--
-- The first application of the loader_checks migration stored the regex
-- without its backslashes:
--   '^d{1,4}(.d{1,4}){0,3}$'
-- so every version, including the current one, failed the check and startup
-- returned {"ok":false,"error":"outdated"}.
--
-- Recreate the function with the intended pattern.
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
