\set VERBOSITY terse

CREATE EXTENSION pg_pathman;
CREATE SCHEMA permissions;

CREATE ROLE user1 LOGIN;
CREATE ROLE user2 LOGIN;

GRANT USAGE, CREATE ON SCHEMA permissions TO user1;
GRANT USAGE, CREATE ON SCHEMA permissions TO user2;

ALTER DEFAULT PRIVILEGES FOR ROLE user1
IN SCHEMA permissions
GRANT SELECT, INSERT ON TABLES
TO user2;

/* Switch to #1 */
SET ROLE user1;
CREATE TABLE permissions.user1_table(id serial, a int);
INSERT INTO permissions.user1_table SELECT g, g FROM generate_series(1, 20) as g;

/* Should fail */
SET ROLE user2;
SELECT create_range_partitions('permissions.user1_table', 'id', 1, 10, 2);

/* Should be ok */
SET ROLE user1;
SELECT create_range_partitions('permissions.user1_table', 'id', 1, 10, 2);

/* Should be able to see */
SET ROLE user2;
SELECT * FROM pathman_config;
SELECT * FROM pathman_config_params;

/* Should fail */
SET ROLE user2;
SELECT set_enable_parent('permissions.user1_table', true);
SELECT set_auto('permissions.user1_table', false);

/* Should fail */
SET ROLE user2;
DELETE FROM pathman_config
WHERE partrel = 'permissions.user1_table'::regclass;

/* No rights to insert, should fail */
SET ROLE user2;
INSERT INTO permissions.user1_table (id, a) VALUES (35, 0);

/* Have rights, should be ok (bgw connects as user1) */
SET ROLE user1;
GRANT INSERT ON permissions.user1_table TO user2;
SET ROLE user2;
INSERT INTO permissions.user1_table (id, a) VALUES (35, 0) RETURNING *;
SELECT relacl FROM pg_class WHERE oid = 'permissions.user1_table_4'::regclass;

/* Try to drop partition, should fail */
SELECT drop_range_partition('permissions.user1_table_4');

/* Disable automatic partition creation */
SET ROLE user1;
SELECT set_auto('permissions.user1_table', false);

/* Partition creation, should fail */
SET ROLE user2;
INSERT INTO permissions.user1_table (id, a) VALUES (55, 0) RETURNING *;

/* Finally drop partitions */
SET ROLE user1;
SELECT drop_partitions('permissions.user1_table');


/* Switch to #2 */
SET ROLE user2;
/* Test ddl event trigger */
CREATE TABLE permissions.user2_table(id serial);
SELECT create_hash_partitions('permissions.user2_table', 'id', 3);
INSERT INTO permissions.user2_table SELECT generate_series(1, 30);
SELECT drop_partitions('permissions.user2_table');


/* Finally reset user */
RESET ROLE;

DROP OWNED BY user1;
DROP OWNED BY user2;
DROP USER user1;
DROP USER user2;


DROP SCHEMA permissions CASCADE;
DROP EXTENSION pg_pathman;
