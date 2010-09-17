/*
 * *** Do not change this file by hand. It is automatically
 * *** generated from the DocBook documentation.
 *
 * generated by
 *     /usr/local/bin/suidperl create_help.pl ../../../doc/src/sgml/ref sql_help.h
 *
 */

#ifndef SQL_HELP_H
#define SQL_HELP_H

#define N_(x) (x)				/* gettext noop */

struct _helpStruct
{
	const char	   *cmd;		/* the command name */
	const char	   *help;		/* the help associated with it */
	const char	   *syntax;		/* the syntax associated with it */
};


static const struct _helpStruct QL_HELP[] = {
    { "ABORT",
      N_("abort the current transaction"),
      N_("ABORT [ WORK | TRANSACTION ]") },

    { "ALTER AGGREGATE",
      N_("change the definition of an aggregate function"),
      N_("ALTER AGGREGATE name ( type [ , ... ] ) RENAME TO new_name\nALTER AGGREGATE name ( type [ , ... ] ) OWNER TO new_owner\nALTER AGGREGATE name ( type [ , ... ] ) SET SCHEMA new_schema") },

    { "ALTER CONVERSION",
      N_("change the definition of a conversion"),
      N_("ALTER CONVERSION name RENAME TO newname\nALTER CONVERSION name OWNER TO newowner") },

    { "ALTER DATABASE",
      N_("change a database"),
      N_("ALTER DATABASE name [ [ WITH ] option [ ... ] ]\n\nwhere option can be:\n\n    CONNECTION LIMIT connlimit\n\nALTER DATABASE name RENAME TO newname\n\nALTER DATABASE name OWNER TO new_owner\n\nALTER DATABASE name SET configuration_parameter { TO | = } { value | DEFAULT }\nALTER DATABASE name SET configuration_parameter FROM CURRENT\nALTER DATABASE name RESET configuration_parameter\nALTER DATABASE name RESET ALL") },

    { "ALTER DOMAIN",
      N_("change the definition of a domain"),
      N_("ALTER DOMAIN name\n    { SET DEFAULT expression | DROP DEFAULT }\nALTER DOMAIN name\n    { SET | DROP } NOT NULL\nALTER DOMAIN name\n    ADD domain_constraint\nALTER DOMAIN name\n    DROP CONSTRAINT constraint_name [ RESTRICT | CASCADE ]\nALTER DOMAIN name\n    OWNER TO new_owner \nALTER DOMAIN name\n    SET SCHEMA new_schema") },

    { "ALTER FUNCTION",
      N_("change the definition of a function"),
      N_("ALTER FUNCTION name ( [ [ argmode ] [ argname ] argtype [, ...] ] )\n    action [ ... ] [ RESTRICT ]\nALTER FUNCTION name ( [ [ argmode ] [ argname ] argtype [, ...] ] )\n    RENAME TO new_name\nALTER FUNCTION name ( [ [ argmode ] [ argname ] argtype [, ...] ] )\n    OWNER TO new_owner\nALTER FUNCTION name ( [ [ argmode ] [ argname ] argtype [, ...] ] )\n    SET SCHEMA new_schema\n\nwhere action is one of:\n\n    CALLED ON NULL INPUT | RETURNS NULL ON NULL INPUT | STRICT\n    IMMUTABLE | STABLE | VOLATILE\n    [ EXTERNAL ] SECURITY INVOKER | [ EXTERNAL ] SECURITY DEFINER\n    COST execution_cost\n    ROWS result_rows\n    SET configuration_parameter { TO | = } { value | DEFAULT }\n    SET configuration_parameter FROM CURRENT\n    RESET configuration_parameter\n    RESET ALL") },

    { "ALTER GROUP",
      N_("change role name or membership"),
      N_("ALTER GROUP groupname ADD USER username [, ... ]\nALTER GROUP groupname DROP USER username [, ... ]\n\nALTER GROUP groupname RENAME TO newname") },

    { "ALTER INDEX",
      N_("change the definition of an index"),
      N_("ALTER INDEX name RENAME TO new_name\nALTER INDEX name SET TABLESPACE tablespace_name\nALTER INDEX name SET ( storage_parameter = value [, ... ] )\nALTER INDEX name RESET ( storage_parameter [, ... ] )") },

    { "ALTER LANGUAGE",
      N_("change the definition of a procedural language"),
      N_("ALTER [ PROCEDURAL ] LANGUAGE name RENAME TO newname\nALTER [ PROCEDURAL ] LANGUAGE name OWNER TO new_owner") },

    { "ALTER OPERATOR CLASS",
      N_("change the definition of an operator class"),
      N_("ALTER OPERATOR CLASS name USING index_method RENAME TO newname\nALTER OPERATOR CLASS name USING index_method OWNER TO newowner") },

    { "ALTER OPERATOR",
      N_("change the definition of an operator"),
      N_("ALTER OPERATOR name ( { lefttype | NONE } , { righttype | NONE } ) OWNER TO newowner") },

    { "ALTER OPERATOR FAMILY",
      N_("change the definition of an operator family"),
      N_("ALTER OPERATOR FAMILY name USING index_method ADD\n  {  OPERATOR strategy_number operator_name ( op_type, op_type ) [ RECHECK ]\n   | FUNCTION support_number [ ( op_type [ , op_type ] ) ] funcname ( argument_type [, ...] )\n  } [, ... ]\nALTER OPERATOR FAMILY name USING index_method DROP\n  {  OPERATOR strategy_number ( op_type [ , op_type ] )\n   | FUNCTION support_number ( op_type [ , op_type ] )\n  } [, ... ]\nALTER OPERATOR FAMILY name USING index_method RENAME TO newname\nALTER OPERATOR FAMILY name USING index_method OWNER TO newowner") },

    { "ALTER ROLE",
      N_("change a database role"),
      N_("ALTER ROLE name [ [ WITH ] option [ ... ] ]\n\nwhere option can be:\n    \n      SUPERUSER | NOSUPERUSER\n    | CREATEDB | NOCREATEDB\n    | CREATEROLE | NOCREATEROLE\n    | CREATEUSER | NOCREATEUSER\n    | INHERIT | NOINHERIT\n    | LOGIN | NOLOGIN\n    | CONNECTION LIMIT connlimit\n    | [ ENCRYPTED | UNENCRYPTED ] PASSWORD 'password'\n    | VALID UNTIL 'timestamp' \n\nALTER ROLE name RENAME TO newname\n\nALTER ROLE name SET configuration_parameter { TO | = } { value | DEFAULT }\nALTER ROLE name SET configuration_parameter FROM CURRENT\nALTER ROLE name RESET configuration_parameter\nALTER ROLE name RESET ALL") },

    { "ALTER SCHEMA",
      N_("change the definition of a schema"),
      N_("ALTER SCHEMA name RENAME TO newname\nALTER SCHEMA name OWNER TO newowner") },

    { "ALTER SEQUENCE",
      N_("change the definition of a sequence generator"),
      N_("ALTER SEQUENCE name [ INCREMENT [ BY ] increment ]\n    [ MINVALUE minvalue | NO MINVALUE ] [ MAXVALUE maxvalue | NO MAXVALUE ]\n    [ RESTART [ WITH ] start ] [ CACHE cache ] [ [ NO ] CYCLE ]\n    [ OWNED BY { table.column | NONE } ]\nALTER SEQUENCE name RENAME TO new_name\nALTER SEQUENCE name SET SCHEMA new_schema") },

    { "ALTER TABLE",
      N_("change the definition of a table"),
      N_("ALTER TABLE [ ONLY ] name [ * ]\n    action [, ... ]\nALTER TABLE [ ONLY ] name [ * ]\n    RENAME [ COLUMN ] column TO new_column\nALTER TABLE name\n    RENAME TO new_name\nALTER TABLE name\n    SET SCHEMA new_schema\n\nwhere action is one of:\n\n    ADD [ COLUMN ] column type [ column_constraint [ ... ] ]\n    DROP [ COLUMN ] column [ RESTRICT | CASCADE ]\n    ALTER [ COLUMN ] column TYPE type [ USING expression ]\n    ALTER [ COLUMN ] column SET DEFAULT expression\n    ALTER [ COLUMN ] column DROP DEFAULT\n    ALTER [ COLUMN ] column { SET | DROP } NOT NULL\n    ALTER [ COLUMN ] column SET STATISTICS integer\n    ALTER [ COLUMN ] column SET STORAGE { PLAIN | EXTERNAL | EXTENDED | MAIN }\n    ADD table_constraint\n    DROP CONSTRAINT constraint_name [ RESTRICT | CASCADE ]\n    DISABLE TRIGGER [ trigger_name | ALL | USER ]\n    ENABLE TRIGGER [ trigger_name | ALL | USER ]\n    ENABLE REPLICA TRIGGER trigger_name\n    ENABLE ALWAYS TRIGGER trigger_name\n    DISABLE RULE rewrite_rule_name\n    ENABLE RULE rewrite_rule_name\n    ENABLE REPLICA RULE rewrite_rule_name\n    ENABLE ALWAYS RULE rewrite_rule_name\n    CLUSTER ON index_name\n    SET WITHOUT CLUSTER\n    SET WITHOUT OIDS\n    SET ( storage_parameter = value [, ... ] )\n    RESET ( storage_parameter [, ... ] )\n    INHERIT parent_table\n    NO INHERIT parent_table\n    OWNER TO new_owner\n    SET TABLESPACE new_tablespace") },

    { "ALTER TABLESPACE",
      N_("change the definition of a tablespace"),
      N_("ALTER TABLESPACE name RENAME TO newname\nALTER TABLESPACE name OWNER TO newowner") },

    { "ALTER TRIGGER",
      N_("change the definition of a trigger"),
      N_("ALTER TRIGGER name ON table RENAME TO newname") },

    { "ALTER TEXT SEARCH CONFIGURATION",
      N_("change the definition of a text search configuration"),
      N_("ALTER TEXT SEARCH CONFIGURATION name\n    ADD MAPPING FOR token_type [, ... ] WITH dictionary_name [, ... ]\nALTER TEXT SEARCH CONFIGURATION name\n    ALTER MAPPING FOR token_type [, ... ] WITH dictionary_name [, ... ]\nALTER TEXT SEARCH CONFIGURATION name\n    ALTER MAPPING REPLACE old_dictionary WITH new_dictionary\nALTER TEXT SEARCH CONFIGURATION name\n    ALTER MAPPING FOR token_type [, ... ] REPLACE old_dictionary WITH new_dictionary\nALTER TEXT SEARCH CONFIGURATION name\n    DROP MAPPING [ IF EXISTS ] FOR token_type [, ... ]\nALTER TEXT SEARCH CONFIGURATION name RENAME TO newname\nALTER TEXT SEARCH CONFIGURATION name OWNER TO newowner") },

    { "ALTER TEXT SEARCH DICTIONARY",
      N_("change the definition of a text search dictionary"),
      N_("ALTER TEXT SEARCH DICTIONARY name (\n    option [ = value ] [, ... ]\n)\nALTER TEXT SEARCH DICTIONARY name RENAME TO newname\nALTER TEXT SEARCH DICTIONARY name OWNER TO newowner") },

    { "ALTER TEXT SEARCH PARSER",
      N_("change the definition of a text search parser"),
      N_("ALTER TEXT SEARCH PARSER name RENAME TO newname") },

    { "ALTER TEXT SEARCH TEMPLATE",
      N_("change the definition of a text search template"),
      N_("ALTER TEXT SEARCH TEMPLATE name RENAME TO newname") },

    { "ALTER TYPE",
      N_("change the definition of a type"),
      N_("ALTER TYPE name OWNER TO new_owner \nALTER TYPE name SET SCHEMA new_schema") },

    { "ALTER USER",
      N_("change a database role"),
      N_("ALTER USER name [ [ WITH ] option [ ... ] ]\n\nwhere option can be:\n    \n      SUPERUSER | NOSUPERUSER\n    | CREATEDB | NOCREATEDB\n    | CREATEROLE | NOCREATEROLE\n    | CREATEUSER | NOCREATEUSER\n    | INHERIT | NOINHERIT\n    | LOGIN | NOLOGIN\n    | CONNECTION LIMIT connlimit\n    | [ ENCRYPTED | UNENCRYPTED ] PASSWORD 'password'\n    | VALID UNTIL 'timestamp' \n\nALTER USER name RENAME TO newname\n\nALTER USER name SET configuration_parameter { TO | = } { value | DEFAULT }\nALTER USER name SET configuration_parameter FROM CURRENT\nALTER USER name RESET configuration_parameter\nALTER USER name RESET ALL") },

    { "ALTER VIEW",
      N_("change the definition of a view"),
      N_("ALTER VIEW name RENAME TO newname") },

    { "ANALYZE",
      N_("collect statistics about a database"),
      N_("ANALYZE [ VERBOSE ] [ table [ ( column [, ...] ) ] ]") },

    { "BEGIN",
      N_("start a transaction block"),
      N_("BEGIN [ WORK | TRANSACTION ] [ transaction_mode [, ...] ]\n\nwhere transaction_mode is one of:\n\n    ISOLATION LEVEL { SERIALIZABLE | REPEATABLE READ | READ COMMITTED | READ UNCOMMITTED }\n    READ WRITE | READ ONLY") },

    { "CHECKPOINT",
      N_("force a transaction log checkpoint"),
      N_("CHECKPOINT") },

    { "CLOSE",
      N_("close a cursor"),
      N_("CLOSE { name | ALL }") },

    { "CLUSTER",
      N_("cluster a table according to an index"),
      N_("CLUSTER tablename [ USING indexname ]\nCLUSTER") },

    { "COMMENT",
      N_("define or change the comment of an object"),
      N_("COMMENT ON\n{\n  TABLE object_name |\n  COLUMN table_name.column_name |\n  AGGREGATE agg_name (agg_type [, ...] ) |\n  CAST (sourcetype AS targettype) |\n  CONSTRAINT constraint_name ON table_name |\n  CONVERSION object_name |\n  DATABASE object_name |\n  DOMAIN object_name |\n  FUNCTION func_name ( [ [ argmode ] [ argname ] argtype [, ...] ] ) |\n  INDEX object_name |\n  LARGE OBJECT large_object_oid |\n  OPERATOR op (leftoperand_type, rightoperand_type) |\n  OPERATOR CLASS object_name USING index_method |\n  OPERATOR FAMILY object_name USING index_method |\n  [ PROCEDURAL ] LANGUAGE object_name |\n  ROLE object_name |\n  RULE rule_name ON table_name |\n  SCHEMA object_name |\n  SEQUENCE object_name |\n  TABLESPACE object_name |\n  TEXT SEARCH CONFIGURATION object_name |\n  TEXT SEARCH DICTIONARY object_name |\n  TEXT SEARCH PARSER object_name |\n  TEXT SEARCH TEMPLATE object_name |\n  TRIGGER trigger_name ON table_name |\n  TYPE object_name |\n  VIEW object_name\n} IS 'text'") },

    { "COMMIT",
      N_("commit the current transaction"),
      N_("COMMIT [ WORK | TRANSACTION ]") },

    { "COMMIT PREPARED",
      N_("commit a transaction that was earlier prepared for two-phase commit"),
      N_("COMMIT PREPARED transaction_id") },

    { "COPY",
      N_("copy data between a file and a table"),
      N_("COPY tablename [ ( column [, ...] ) ]\n    FROM { 'filename' | STDIN }\n    [ [ WITH ] \n          [ BINARY ]\n          [ OIDS ]\n          [ DELIMITER [ AS ] 'delimiter' ]\n          [ NULL [ AS ] 'null string' ]\n          [ CSV [ HEADER ]\n                [ QUOTE [ AS ] 'quote' ] \n                [ ESCAPE [ AS ] 'escape' ]\n                [ FORCE NOT NULL column [, ...] ]\n\nCOPY { tablename [ ( column [, ...] ) ] | ( query ) }\n    TO { 'filename' | STDOUT }\n    [ [ WITH ] \n          [ BINARY ]\n          [ OIDS ]\n          [ DELIMITER [ AS ] 'delimiter' ]\n          [ NULL [ AS ] 'null string' ]\n          [ CSV [ HEADER ]\n                [ QUOTE [ AS ] 'quote' ] \n                [ ESCAPE [ AS ] 'escape' ]\n                [ FORCE QUOTE column [, ...] ]") },

    { "CREATE AGGREGATE",
      N_("define a new aggregate function"),
      N_("CREATE AGGREGATE name ( input_data_type [ , ... ] ) (\n    SFUNC = sfunc,\n    STYPE = state_data_type\n    [ , FINALFUNC = ffunc ]\n    [ , INITCOND = initial_condition ]\n    [ , SORTOP = sort_operator ]\n)\n\nor the old syntax\n\nCREATE AGGREGATE name (\n    BASETYPE = base_type,\n    SFUNC = sfunc,\n    STYPE = state_data_type\n    [ , FINALFUNC = ffunc ]\n    [ , INITCOND = initial_condition ]\n    [ , SORTOP = sort_operator ]\n)") },

    { "CREATE CAST",
      N_("define a new cast"),
      N_("CREATE CAST (sourcetype AS targettype)\n    WITH FUNCTION funcname (argtypes)\n    [ AS ASSIGNMENT | AS IMPLICIT ]\n\nCREATE CAST (sourcetype AS targettype)\n    WITHOUT FUNCTION\n    [ AS ASSIGNMENT | AS IMPLICIT ]") },

    { "CREATE CONSTRAINT TRIGGER",
      N_("define a new constraint trigger"),
      N_("CREATE CONSTRAINT TRIGGER name\n    AFTER event [ OR ... ]\n    ON table_name\n    [ FROM referenced_table_name ]\n    { NOT DEFERRABLE | [ DEFERRABLE ] { INITIALLY IMMEDIATE | INITIALLY DEFERRED } }\n    FOR EACH ROW\n    EXECUTE PROCEDURE funcname ( arguments )") },

    { "CREATE CONVERSION",
      N_("define a new encoding conversion"),
      N_("CREATE [ DEFAULT ] CONVERSION name\n    FOR source_encoding TO dest_encoding FROM funcname") },

    { "CREATE DATABASE",
      N_("create a new database"),
      N_("CREATE DATABASE name\n    [ [ WITH ] [ OWNER [=] dbowner ]\n           [ TEMPLATE [=] template ]\n           [ ENCODING [=] encoding ]\n           [ TABLESPACE [=] tablespace ]\n           [ CONNECTION LIMIT [=] connlimit ] ]") },

    { "CREATE DOMAIN",
      N_("define a new domain"),
      N_("CREATE DOMAIN name [ AS ] data_type\n    [ DEFAULT expression ]\n    [ constraint [ ... ] ]\n\nwhere constraint is:\n\n[ CONSTRAINT constraint_name ]\n{ NOT NULL | NULL | CHECK (expression) }") },

    { "CREATE FUNCTION",
      N_("define a new function"),
      N_("CREATE [ OR REPLACE ] FUNCTION\n    name ( [ [ argmode ] [ argname ] argtype [, ...] ] )\n    [ RETURNS rettype ]\n  { LANGUAGE langname\n    | IMMUTABLE | STABLE | VOLATILE\n    | CALLED ON NULL INPUT | RETURNS NULL ON NULL INPUT | STRICT\n    | [ EXTERNAL ] SECURITY INVOKER | [ EXTERNAL ] SECURITY DEFINER\n    | COST execution_cost\n    | ROWS result_rows\n    | SET configuration_parameter { TO value | = value | FROM CURRENT }\n    | AS 'definition'\n    | AS 'obj_file', 'link_symbol'\n  } ...\n    [ WITH ( attribute [, ...] ) ]") },

    { "CREATE GROUP",
      N_("define a new database role"),
      N_("CREATE GROUP name [ [ WITH ] option [ ... ] ]\n\nwhere option can be:\n    \n      SUPERUSER | NOSUPERUSER\n    | CREATEDB | NOCREATEDB\n    | CREATEROLE | NOCREATEROLE\n    | CREATEUSER | NOCREATEUSER\n    | INHERIT | NOINHERIT\n    | LOGIN | NOLOGIN\n    | [ ENCRYPTED | UNENCRYPTED ] PASSWORD 'password'\n    | VALID UNTIL 'timestamp' \n    | IN ROLE rolename [, ...]\n    | IN GROUP rolename [, ...]\n    | ROLE rolename [, ...]\n    | ADMIN rolename [, ...]\n    | USER rolename [, ...]\n    | SYSID uid") },

    { "CREATE INDEX",
      N_("define a new index"),
      N_("CREATE [ UNIQUE ] INDEX [ CONCURRENTLY ] name ON table [ USING method ]\n    ( { column | ( expression ) } [ opclass ] [ ASC | DESC ] [ NULLS { FIRST | LAST } ] [, ...] )\n    [ WITH ( storage_parameter = value [, ... ] ) ]\n    [ TABLESPACE tablespace ]\n    [ WHERE predicate ]") },

    { "CREATE LANGUAGE",
      N_("define a new procedural language"),
      N_("CREATE [ PROCEDURAL ] LANGUAGE name\nCREATE [ TRUSTED ] [ PROCEDURAL ] LANGUAGE name\n    HANDLER call_handler [ VALIDATOR valfunction ]") },

    { "CREATE OPERATOR CLASS",
      N_("define a new operator class"),
      N_("CREATE OPERATOR CLASS name [ DEFAULT ] FOR TYPE data_type\n  USING index_method [ FAMILY family_name ] AS\n  {  OPERATOR strategy_number operator_name [ ( op_type, op_type ) ] [ RECHECK ]\n   | FUNCTION support_number [ ( op_type [ , op_type ] ) ] funcname ( argument_type [, ...] )\n   | STORAGE storage_type\n  } [, ... ]") },

    { "CREATE OPERATOR",
      N_("define a new operator"),
      N_("CREATE OPERATOR name (\n    PROCEDURE = funcname\n    [, LEFTARG = lefttype ] [, RIGHTARG = righttype ]\n    [, COMMUTATOR = com_op ] [, NEGATOR = neg_op ]\n    [, RESTRICT = res_proc ] [, JOIN = join_proc ]\n    [, HASHES ] [, MERGES ]\n)") },

    { "CREATE OPERATOR FAMILY",
      N_("define a new operator family"),
      N_("CREATE OPERATOR FAMILY name USING index_method") },

    { "CREATE ROLE",
      N_("define a new database role"),
      N_("CREATE ROLE name [ [ WITH ] option [ ... ] ]\n\nwhere option can be:\n    \n      SUPERUSER | NOSUPERUSER\n    | CREATEDB | NOCREATEDB\n    | CREATEROLE | NOCREATEROLE\n    | CREATEUSER | NOCREATEUSER\n    | INHERIT | NOINHERIT\n    | LOGIN | NOLOGIN\n    | CONNECTION LIMIT connlimit\n    | [ ENCRYPTED | UNENCRYPTED ] PASSWORD 'password'\n    | VALID UNTIL 'timestamp' \n    | IN ROLE rolename [, ...]\n    | IN GROUP rolename [, ...]\n    | ROLE rolename [, ...]\n    | ADMIN rolename [, ...]\n    | USER rolename [, ...]\n    | SYSID uid") },

    { "CREATE RULE",
      N_("define a new rewrite rule"),
      N_("CREATE [ OR REPLACE ] RULE name AS ON event\n    TO table [ WHERE condition ]\n    DO [ ALSO | INSTEAD ] { NOTHING | command | ( command ; command ... ) }") },

    { "CREATE SCHEMA",
      N_("define a new schema"),
      N_("CREATE SCHEMA schemaname [ AUTHORIZATION username ] [ schema_element [ ... ] ]\nCREATE SCHEMA AUTHORIZATION username [ schema_element [ ... ] ]") },

    { "CREATE SEQUENCE",
      N_("define a new sequence generator"),
      N_("CREATE [ TEMPORARY | TEMP ] SEQUENCE name [ INCREMENT [ BY ] increment ]\n    [ MINVALUE minvalue | NO MINVALUE ] [ MAXVALUE maxvalue | NO MAXVALUE ]\n    [ START [ WITH ] start ] [ CACHE cache ] [ [ NO ] CYCLE ]\n    [ OWNED BY { table.column | NONE } ]") },

    { "CREATE TABLE",
      N_("define a new table"),
      N_("CREATE [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } ] TABLE table_name ( [\n  { column_name data_type [ DEFAULT default_expr ] [ column_constraint [ ... ] ]\n    | table_constraint\n    | LIKE parent_table [ { INCLUDING | EXCLUDING } { DEFAULTS | CONSTRAINTS | INDEXES } ] ... }\n    [, ... ]\n] )\n[ INHERITS ( parent_table [, ... ] ) ]\n[ WITH ( storage_parameter [= value] [, ... ] ) | WITH OIDS | WITHOUT OIDS ]\n[ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]\n[ TABLESPACE tablespace ]\n\nwhere column_constraint is:\n\n[ CONSTRAINT constraint_name ]\n{ NOT NULL | \n  NULL | \n  UNIQUE index_parameters |\n  PRIMARY KEY index_parameters |\n  CHECK ( expression ) |\n  REFERENCES reftable [ ( refcolumn ) ] [ MATCH FULL | MATCH PARTIAL | MATCH SIMPLE ]\n    [ ON DELETE action ] [ ON UPDATE action ] }\n[ DEFERRABLE | NOT DEFERRABLE ] [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]\n\nand table_constraint is:\n\n[ CONSTRAINT constraint_name ]\n{ UNIQUE ( column_name [, ... ] ) index_parameters |\n  PRIMARY KEY ( column_name [, ... ] ) index_parameters |\n  CHECK ( expression ) |\n  FOREIGN KEY ( column_name [, ... ] ) REFERENCES reftable [ ( refcolumn [, ... ] ) ]\n    [ MATCH FULL | MATCH PARTIAL | MATCH SIMPLE ] [ ON DELETE action ] [ ON UPDATE action ] }\n[ DEFERRABLE | NOT DEFERRABLE ] [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]\n\nindex_parameters in UNIQUE and PRIMARY KEY constraints are:\n\n[ WITH ( storage_parameter [= value] [, ... ] ) ]\n[ USING INDEX TABLESPACE tablespace ]") },

    { "CREATE TABLE AS",
      N_("define a new table from the results of a query"),
      N_("CREATE [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } ] TABLE table_name\n    [ (column_name [, ...] ) ]\n    [ WITH ( storage_parameter [= value] [, ... ] ) | WITH OIDS | WITHOUT OIDS ]\n    [ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]\n    [ TABLESPACE tablespace ]\n    AS query") },

    { "CREATE TABLESPACE",
      N_("define a new tablespace"),
      N_("CREATE TABLESPACE tablespacename [ OWNER username ] LOCATION 'directory'") },

    { "CREATE TRIGGER",
      N_("define a new trigger"),
      N_("CREATE TRIGGER name { BEFORE | AFTER } { event [ OR ... ] }\n    ON table [ FOR [ EACH ] { ROW | STATEMENT } ]\n    EXECUTE PROCEDURE funcname ( arguments )") },

    { "CREATE TEXT SEARCH CONFIGURATION",
      N_("define a new text search configuration"),
      N_("CREATE TEXT SEARCH CONFIGURATION name (\n    PARSER = parser_name |\n    COPY = source_config\n)") },

    { "CREATE TEXT SEARCH DICTIONARY",
      N_("define a new text search dictionary"),
      N_("CREATE TEXT SEARCH DICTIONARY name (\n    TEMPLATE = template\n    [, option = value [, ... ]]\n)") },

    { "CREATE TEXT SEARCH PARSER",
      N_("define a new text search parser"),
      N_("CREATE TEXT SEARCH PARSER name (\n    START = start_function ,\n    GETTOKEN = gettoken_function ,\n    END = end_function ,\n    LEXTYPES = lextypes_function\n    [, HEADLINE = headline_function ]\n)") },

    { "CREATE TEXT SEARCH TEMPLATE",
      N_("define a new text search template"),
      N_("CREATE TEXT SEARCH TEMPLATE name (\n    [ INIT = init_function , ]\n    LEXIZE = lexize_function\n)") },

    { "CREATE TYPE",
      N_("define a new data type"),
      N_("CREATE TYPE name AS\n    ( attribute_name data_type [, ... ] )\n\nCREATE TYPE name AS ENUM\n    ( 'label' [, ... ] )\n\nCREATE TYPE name (\n    INPUT = input_function,\n    OUTPUT = output_function\n    [ , RECEIVE = receive_function ]\n    [ , SEND = send_function ]\n    [ , TYPMOD_IN = type_modifier_input_function ]\n    [ , TYPMOD_OUT = type_modifier_output_function ]\n    [ , ANALYZE = analyze_function ]\n    [ , INTERNALLENGTH = { internallength | VARIABLE } ]\n    [ , PASSEDBYVALUE ]\n    [ , ALIGNMENT = alignment ]\n    [ , STORAGE = storage ]\n    [ , DEFAULT = default ]\n    [ , ELEMENT = element ]\n    [ , DELIMITER = delimiter ]\n)\n\nCREATE TYPE name") },

    { "CREATE USER",
      N_("define a new database role"),
      N_("CREATE USER name [ [ WITH ] option [ ... ] ]\n\nwhere option can be:\n    \n      SUPERUSER | NOSUPERUSER\n    | CREATEDB | NOCREATEDB\n    | CREATEROLE | NOCREATEROLE\n    | CREATEUSER | NOCREATEUSER\n    | INHERIT | NOINHERIT\n    | LOGIN | NOLOGIN\n    | CONNECTION LIMIT connlimit\n    | [ ENCRYPTED | UNENCRYPTED ] PASSWORD 'password'\n    | VALID UNTIL 'timestamp' \n    | IN ROLE rolename [, ...]\n    | IN GROUP rolename [, ...]\n    | ROLE rolename [, ...]\n    | ADMIN rolename [, ...]\n    | USER rolename [, ...]\n    | SYSID uid") },

    { "CREATE VIEW",
      N_("define a new view"),
      N_("CREATE [ OR REPLACE ] [ TEMP | TEMPORARY ] VIEW name [ ( column_name [, ...] ) ]\n    AS query") },

    { "DEALLOCATE",
      N_("deallocate a prepared statement"),
      N_("DEALLOCATE [ PREPARE ] { name | ALL }") },

    { "DECLARE",
      N_("define a cursor"),
      N_("DECLARE name [ BINARY ] [ INSENSITIVE ] [ [ NO ] SCROLL ]\n    CURSOR [ { WITH | WITHOUT } HOLD ] FOR query") },

    { "DELETE",
      N_("delete rows of a table"),
      N_("DELETE FROM [ ONLY ] table [ [ AS ] alias ]\n    [ USING usinglist ]\n    [ WHERE condition | WHERE CURRENT OF cursor_name ]\n    [ RETURNING * | output_expression [ AS output_name ] [, ...] ]") },

    { "DISCARD",
      N_("discard session state"),
      N_("DISCARD { ALL | PLANS | TEMPORARY | TEMP }") },

    { "DROP AGGREGATE",
      N_("remove an aggregate function"),
      N_("DROP AGGREGATE [ IF EXISTS ] name ( type [ , ... ] ) [ CASCADE | RESTRICT ]") },

    { "DROP CAST",
      N_("remove a cast"),
      N_("DROP CAST [ IF EXISTS ] (sourcetype AS targettype) [ CASCADE | RESTRICT ]") },

    { "DROP CONVERSION",
      N_("remove a conversion"),
      N_("DROP CONVERSION [ IF EXISTS ] name [ CASCADE | RESTRICT ]") },

    { "DROP DATABASE",
      N_("remove a database"),
      N_("DROP DATABASE [ IF EXISTS ] name") },

    { "DROP DOMAIN",
      N_("remove a domain"),
      N_("DROP DOMAIN [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]") },

    { "DROP FUNCTION",
      N_("remove a function"),
      N_("DROP FUNCTION [ IF EXISTS ] name ( [ [ argmode ] [ argname ] argtype [, ...] ] )\n    [ CASCADE | RESTRICT ]") },

    { "DROP GROUP",
      N_("remove a database role"),
      N_("DROP GROUP [ IF EXISTS ] name [, ...]") },

    { "DROP INDEX",
      N_("remove an index"),
      N_("DROP INDEX [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]") },

    { "DROP LANGUAGE",
      N_("remove a procedural language"),
      N_("DROP [ PROCEDURAL ] LANGUAGE [ IF EXISTS ] name [ CASCADE | RESTRICT ]") },

    { "DROP OPERATOR CLASS",
      N_("remove an operator class"),
      N_("DROP OPERATOR CLASS [ IF EXISTS ] name USING index_method [ CASCADE | RESTRICT ]") },

    { "DROP OPERATOR",
      N_("remove an operator"),
      N_("DROP OPERATOR [ IF EXISTS ] name ( { lefttype | NONE } , { righttype | NONE } ) [ CASCADE | RESTRICT ]") },

    { "DROP OPERATOR FAMILY",
      N_("remove an operator family"),
      N_("DROP OPERATOR FAMILY [ IF EXISTS ] name USING index_method [ CASCADE | RESTRICT ]") },

    { "DROP OWNED",
      N_("remove database objects owned by a database role"),
      N_("DROP OWNED BY name [, ...] [ CASCADE | RESTRICT ]") },

    { "DROP ROLE",
      N_("remove a database role"),
      N_("DROP ROLE [ IF EXISTS ] name [, ...]") },

    { "DROP RULE",
      N_("remove a rewrite rule"),
      N_("DROP RULE [ IF EXISTS ] name ON relation [ CASCADE | RESTRICT ]") },

    { "DROP SCHEMA",
      N_("remove a schema"),
      N_("DROP SCHEMA [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]") },

    { "DROP SEQUENCE",
      N_("remove a sequence"),
      N_("DROP SEQUENCE [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]") },

    { "DROP TABLE",
      N_("remove a table"),
      N_("DROP TABLE [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]") },

    { "DROP TABLESPACE",
      N_("remove a tablespace"),
      N_("DROP TABLESPACE [ IF EXISTS ] tablespacename") },

    { "DROP TRIGGER",
      N_("remove a trigger"),
      N_("DROP TRIGGER [ IF EXISTS ] name ON table [ CASCADE | RESTRICT ]") },

    { "DROP TEXT SEARCH CONFIGURATION",
      N_("remove a text search configuration"),
      N_("DROP TEXT SEARCH CONFIGURATION [ IF EXISTS ] name [ CASCADE | RESTRICT ]") },

    { "DROP TEXT SEARCH DICTIONARY",
      N_("remove a text search dictionary"),
      N_("DROP TEXT SEARCH DICTIONARY [ IF EXISTS ] name [ CASCADE | RESTRICT ]") },

    { "DROP TEXT SEARCH PARSER",
      N_("remove a text search parser"),
      N_("DROP TEXT SEARCH PARSER [ IF EXISTS ] name [ CASCADE | RESTRICT ]") },

    { "DROP TEXT SEARCH TEMPLATE",
      N_("remove a text search template"),
      N_("DROP TEXT SEARCH TEMPLATE [ IF EXISTS ] name [ CASCADE | RESTRICT ]") },

    { "DROP TYPE",
      N_("remove a data type"),
      N_("DROP TYPE [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]") },

    { "DROP USER",
      N_("remove a database role"),
      N_("DROP USER [ IF EXISTS ] name [, ...]") },

    { "DROP VIEW",
      N_("remove a view"),
      N_("DROP VIEW [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]") },

    { "END",
      N_("commit the current transaction"),
      N_("END [ WORK | TRANSACTION ]") },

    { "EXECUTE",
      N_("execute a prepared statement"),
      N_("EXECUTE name [ ( parameter [, ...] ) ]") },

    { "EXPLAIN",
      N_("show the execution plan of a statement"),
      N_("EXPLAIN [ ANALYZE ] [ VERBOSE ] statement") },

    { "FETCH",
      N_("retrieve rows from a query using a cursor"),
      N_("FETCH [ direction { FROM | IN } ] cursorname\n\nwhere direction can be empty or one of:\n\n    NEXT\n    PRIOR\n    FIRST\n    LAST\n    ABSOLUTE count\n    RELATIVE count\n    count\n    ALL\n    FORWARD\n    FORWARD count\n    FORWARD ALL\n    BACKWARD\n    BACKWARD count\n    BACKWARD ALL") },

    { "GRANT",
      N_("define access privileges"),
      N_("GRANT { { SELECT | INSERT | UPDATE | DELETE | REFERENCES | TRIGGER }\n    [,...] | ALL [ PRIVILEGES ] }\n    ON [ TABLE ] tablename [, ...]\n    TO { [ GROUP ] rolename | PUBLIC } [, ...] [ WITH GRANT OPTION ]\n\nGRANT { { USAGE | SELECT | UPDATE }\n    [,...] | ALL [ PRIVILEGES ] }\n    ON SEQUENCE sequencename [, ...]\n    TO { [ GROUP ] rolename | PUBLIC } [, ...] [ WITH GRANT OPTION ]\n\nGRANT { { CREATE | CONNECT | TEMPORARY | TEMP } [,...] | ALL [ PRIVILEGES ] }\n    ON DATABASE dbname [, ...]\n    TO { [ GROUP ] rolename | PUBLIC } [, ...] [ WITH GRANT OPTION ]\n\nGRANT { EXECUTE | ALL [ PRIVILEGES ] }\n    ON FUNCTION funcname ( [ [ argmode ] [ argname ] argtype [, ...] ] ) [, ...]\n    TO { [ GROUP ] rolename | PUBLIC } [, ...] [ WITH GRANT OPTION ]\n\nGRANT { USAGE | ALL [ PRIVILEGES ] }\n    ON LANGUAGE langname [, ...]\n    TO { [ GROUP ] rolename | PUBLIC } [, ...] [ WITH GRANT OPTION ]\n\nGRANT { { CREATE | USAGE } [,...] | ALL [ PRIVILEGES ] }\n    ON SCHEMA schemaname [, ...]\n    TO { [ GROUP ] rolename | PUBLIC } [, ...] [ WITH GRANT OPTION ]\n\nGRANT { CREATE | ALL [ PRIVILEGES ] }\n    ON TABLESPACE tablespacename [, ...]\n    TO { [ GROUP ] rolename | PUBLIC } [, ...] [ WITH GRANT OPTION ]\n\nGRANT role [, ...] TO rolename [, ...] [ WITH ADMIN OPTION ]") },

    { "INSERT",
      N_("create new rows in a table"),
      N_("INSERT INTO table [ ( column [, ...] ) ]\n    { DEFAULT VALUES | VALUES ( { expression | DEFAULT } [, ...] ) [, ...] | query }\n    [ RETURNING * | output_expression [ AS output_name ] [, ...] ]") },

    { "LISTEN",
      N_("listen for a notification"),
      N_("LISTEN name") },

    { "LOAD",
      N_("load a shared library file"),
      N_("LOAD 'filename'") },

    { "LOCK",
      N_("lock a table"),
      N_("LOCK [ TABLE ] name [, ...] [ IN lockmode MODE ] [ NOWAIT ]\n\nwhere lockmode is one of:\n\n    ACCESS SHARE | ROW SHARE | ROW EXCLUSIVE | SHARE UPDATE EXCLUSIVE\n    | SHARE | SHARE ROW EXCLUSIVE | EXCLUSIVE | ACCESS EXCLUSIVE") },

    { "MOVE",
      N_("position a cursor"),
      N_("MOVE [ direction { FROM | IN } ] cursorname") },

    { "NOTIFY",
      N_("generate a notification"),
      N_("NOTIFY name") },

    { "PREPARE",
      N_("prepare a statement for execution"),
      N_("PREPARE name [ ( datatype [, ...] ) ] AS statement") },

    { "PREPARE TRANSACTION",
      N_("prepare the current transaction for two-phase commit"),
      N_("PREPARE TRANSACTION transaction_id") },

    { "REASSIGN OWNED",
      N_("change the ownership of database objects owned by a database role"),
      N_("REASSIGN OWNED BY old_role [, ...] TO new_role") },

    { "REINDEX",
      N_("rebuild indexes"),
      N_("REINDEX { INDEX | TABLE | DATABASE | SYSTEM } name [ FORCE ]") },

    { "RELEASE SAVEPOINT",
      N_("destroy a previously defined savepoint"),
      N_("RELEASE [ SAVEPOINT ] savepoint_name") },

    { "RESET",
      N_("restore the value of a run-time parameter to the default value"),
      N_("RESET configuration_parameter\nRESET ALL") },

    { "REVOKE",
      N_("remove access privileges"),
      N_("REVOKE [ GRANT OPTION FOR ]\n    { { SELECT | INSERT | UPDATE | DELETE | REFERENCES | TRIGGER }\n    [,...] | ALL [ PRIVILEGES ] }\n    ON [ TABLE ] tablename [, ...]\n    FROM { [ GROUP ] rolename | PUBLIC } [, ...]\n    [ CASCADE | RESTRICT ]\n\nREVOKE [ GRANT OPTION FOR ]\n    { { USAGE | SELECT | UPDATE }\n    [,...] | ALL [ PRIVILEGES ] }\n    ON SEQUENCE sequencename [, ...]\n    FROM { [ GROUP ] rolename | PUBLIC } [, ...]\n    [ CASCADE | RESTRICT ]\n\nREVOKE [ GRANT OPTION FOR ]\n    { { CREATE | CONNECT | TEMPORARY | TEMP } [,...] | ALL [ PRIVILEGES ] }\n    ON DATABASE dbname [, ...]\n    FROM { [ GROUP ] rolename | PUBLIC } [, ...]\n    [ CASCADE | RESTRICT ]\n\nREVOKE [ GRANT OPTION FOR ]\n    { EXECUTE | ALL [ PRIVILEGES ] }\n    ON FUNCTION funcname ( [ [ argmode ] [ argname ] argtype [, ...] ] ) [, ...]\n    FROM { [ GROUP ] rolename | PUBLIC } [, ...]\n    [ CASCADE | RESTRICT ]\n\nREVOKE [ GRANT OPTION FOR ]\n    { USAGE | ALL [ PRIVILEGES ] }\n    ON LANGUAGE langname [, ...]\n    FROM { [ GROUP ] rolename | PUBLIC } [, ...]\n    [ CASCADE | RESTRICT ]\n\nREVOKE [ GRANT OPTION FOR ]\n    { { CREATE | USAGE } [,...] | ALL [ PRIVILEGES ] }\n    ON SCHEMA schemaname [, ...]\n    FROM { [ GROUP ] rolename | PUBLIC } [, ...]\n    [ CASCADE | RESTRICT ]\n\nREVOKE [ GRANT OPTION FOR ]\n    { CREATE | ALL [ PRIVILEGES ] }\n    ON TABLESPACE tablespacename [, ...]\n    FROM { [ GROUP ] rolename | PUBLIC } [, ...]\n    [ CASCADE | RESTRICT ]\n\nREVOKE [ ADMIN OPTION FOR ]\n    role [, ...] FROM rolename [, ...]\n    [ CASCADE | RESTRICT ]") },

    { "ROLLBACK",
      N_("abort the current transaction"),
      N_("ROLLBACK [ WORK | TRANSACTION ]") },

    { "ROLLBACK PREPARED",
      N_("cancel a transaction that was earlier prepared for two-phase commit"),
      N_("ROLLBACK PREPARED transaction_id") },

    { "ROLLBACK TO SAVEPOINT",
      N_("roll back to a savepoint"),
      N_("ROLLBACK [ WORK | TRANSACTION ] TO [ SAVEPOINT ] savepoint_name") },

    { "SAVEPOINT",
      N_("define a new savepoint within the current transaction"),
      N_("SAVEPOINT savepoint_name") },

    { "SELECT",
      N_("retrieve rows from a table or view"),
      N_("SELECT [ ALL | DISTINCT [ ON ( expression [, ...] ) ] ]\n    * | expression [ AS output_name ] [, ...]\n    [ FROM from_item [, ...] ]\n    [ WHERE condition ]\n    [ GROUP BY expression [, ...] ]\n    [ HAVING condition [, ...] ]\n    [ { UNION | INTERSECT | EXCEPT } [ ALL ] select ]\n    [ ORDER BY expression [ ASC | DESC | USING operator ] [ NULLS { FIRST | LAST } ] [, ...] ]\n    [ LIMIT { count | ALL } ]\n    [ OFFSET start ]\n    [ FOR { UPDATE | SHARE } [ OF table_name [, ...] ] [ NOWAIT ] [...] ]\n\nwhere from_item can be one of:\n\n    [ ONLY ] table_name [ * ] [ [ AS ] alias [ ( column_alias [, ...] ) ] ]\n    ( select ) [ AS ] alias [ ( column_alias [, ...] ) ]\n    function_name ( [ argument [, ...] ] ) [ AS ] alias [ ( column_alias [, ...] | column_definition [, ...] ) ]\n    function_name ( [ argument [, ...] ] ) AS ( column_definition [, ...] )\n    from_item [ NATURAL ] join_type from_item [ ON join_condition | USING ( join_column [, ...] ) ]") },

    { "SELECT INTO",
      N_("define a new table from the results of a query"),
      N_("SELECT [ ALL | DISTINCT [ ON ( expression [, ...] ) ] ]\n    * | expression [ AS output_name ] [, ...]\n    INTO [ TEMPORARY | TEMP ] [ TABLE ] new_table\n    [ FROM from_item [, ...] ]\n    [ WHERE condition ]\n    [ GROUP BY expression [, ...] ]\n    [ HAVING condition [, ...] ]\n    [ { UNION | INTERSECT | EXCEPT } [ ALL ] select ]\n    [ ORDER BY expression [ ASC | DESC | USING operator ] [ NULLS { FIRST | LAST } ] [, ...] ]\n    [ LIMIT { count | ALL } ]\n    [ OFFSET start ]\n    [ FOR { UPDATE | SHARE } [ OF table_name [, ...] ] [ NOWAIT ] [...] ]") },

    { "SET",
      N_("change a run-time parameter"),
      N_("SET [ SESSION | LOCAL ] configuration_parameter { TO | = } { value | 'value' | DEFAULT }\nSET [ SESSION | LOCAL ] TIME ZONE { timezone | LOCAL | DEFAULT }") },

    { "SET CONSTRAINTS",
      N_("set constraint checking modes for the current transaction"),
      N_("SET CONSTRAINTS { ALL | name [, ...] } { DEFERRED | IMMEDIATE }") },

    { "SET ROLE",
      N_("set the current user identifier of the current session"),
      N_("SET [ SESSION | LOCAL ] ROLE rolename\nSET [ SESSION | LOCAL ] ROLE NONE\nRESET ROLE") },

    { "SET SESSION AUTHORIZATION",
      N_("set the session user identifier and the current user identifier of the current session"),
      N_("SET [ SESSION | LOCAL ] SESSION AUTHORIZATION username\nSET [ SESSION | LOCAL ] SESSION AUTHORIZATION DEFAULT\nRESET SESSION AUTHORIZATION") },

    { "SET TRANSACTION",
      N_("set the characteristics of the current transaction"),
      N_("SET TRANSACTION transaction_mode [, ...]\nSET SESSION CHARACTERISTICS AS TRANSACTION transaction_mode [, ...]\n\nwhere transaction_mode is one of:\n\n    ISOLATION LEVEL { SERIALIZABLE | REPEATABLE READ | READ COMMITTED | READ UNCOMMITTED }\n    READ WRITE | READ ONLY") },

    { "SHOW",
      N_("show the value of a run-time parameter"),
      N_("SHOW name\nSHOW ALL") },

    { "START TRANSACTION",
      N_("start a transaction block"),
      N_("START TRANSACTION [ transaction_mode [, ...] ]\n\nwhere transaction_mode is one of:\n\n    ISOLATION LEVEL { SERIALIZABLE | REPEATABLE READ | READ COMMITTED | READ UNCOMMITTED }\n    READ WRITE | READ ONLY") },

    { "TRUNCATE",
      N_("empty a table or set of tables"),
      N_("TRUNCATE [ TABLE ] name [, ...] [ CASCADE | RESTRICT ]") },

    { "UNLISTEN",
      N_("stop listening for a notification"),
      N_("UNLISTEN { name | * }") },

    { "UPDATE",
      N_("update rows of a table"),
      N_("UPDATE [ ONLY ] table [ [ AS ] alias ]\n    SET { column = { expression | DEFAULT } |\n          ( column [, ...] ) = ( { expression | DEFAULT } [, ...] ) } [, ...]\n    [ FROM fromlist ]\n    [ WHERE condition | WHERE CURRENT OF cursor_name ]\n    [ RETURNING * | output_expression [ AS output_name ] [, ...] ]") },

    { "VACUUM",
      N_("garbage-collect and optionally analyze a database"),
      N_("VACUUM [ FULL ] [ FREEZE ] [ VERBOSE ] [ table ]\nVACUUM [ FULL ] [ FREEZE ] [ VERBOSE ] ANALYZE [ table [ (column [, ...] ) ] ]") },

    { "VALUES",
      N_("compute a set of rows"),
      N_("VALUES ( expression [, ...] ) [, ...]\n    [ ORDER BY sort_expression [ ASC | DESC | USING operator ] [, ...] ]\n    [ LIMIT { count | ALL } ]\n    [ OFFSET start ]") },


    { NULL, NULL, NULL }    /* End of list marker */
};


#define QL_HELP_COUNT	129		/* number of help items */
#define QL_MAX_CMD_LEN	32		/* largest strlen(cmd) */


#endif /* SQL_HELP_H */
