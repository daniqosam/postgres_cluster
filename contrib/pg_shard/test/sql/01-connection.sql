-- ===================================================================
-- create test functions
-- ===================================================================

CREATE FUNCTION initialize_remote_temp_table(cstring, integer)
	RETURNS bool
	AS 'pg_shard'
	LANGUAGE C STRICT;

CREATE FUNCTION count_remote_temp_table_rows(cstring, integer)
	RETURNS integer
	AS 'pg_shard'
	LANGUAGE C STRICT;

CREATE FUNCTION get_and_purge_connection(cstring, integer)
	RETURNS bool
	AS 'pg_shard'
	LANGUAGE C STRICT;

-- ===================================================================
-- test connection hash functionality
-- ===================================================================

-- reduce verbosity to squelch chatty warnings
\set VERBOSITY terse

-- connect to non-existent host
SELECT initialize_remote_temp_table('dummy-host-name', 12345);

\set VERBOSITY default

-- try to use hostname over 255 characters
SELECT initialize_remote_temp_table(repeat('a', 256)::cstring, :worker_port);

-- connect to localhost and build a temp table
SELECT initialize_remote_temp_table('localhost', :worker_port);

-- table should still be visible since session is reused
SELECT count_remote_temp_table_rows('localhost', :worker_port);

-- purge existing connection to localhost
SELECT get_and_purge_connection('localhost', :worker_port);

-- squelch WARNINGs that contain worker_port
SET client_min_messages TO ERROR;

-- should not be able to see table anymore
SELECT count_remote_temp_table_rows('localhost', :worker_port);

-- recreate once more
SELECT initialize_remote_temp_table('localhost', :worker_port);

-- kill backend to disconnect
SELECT pg_terminate_backend(pid)
	FROM pg_stat_activity
	WHERE application_name = 'pg_shard';

-- should get connection failure (cached connection bad)
SELECT count_remote_temp_table_rows('localhost', :worker_port);

-- should get result failure (reconnected, so no temp table)
SELECT count_remote_temp_table_rows('localhost', :worker_port);

SET client_min_messages TO DEFAULT;
