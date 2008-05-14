INSERT INTO version (table_name, table_version) values ('userblacklist','1');
CREATE TABLE userblacklist (
    id SERIAL PRIMARY KEY NOT NULL,
    username VARCHAR(64) DEFAULT '' NOT NULL,
    domain VARCHAR(64) DEFAULT '' NOT NULL,
    prefix VARCHAR(64) DEFAULT '' NOT NULL,
    whitelist SMALLINT DEFAULT 0 NOT NULL,
    description VARCHAR(64) DEFAULT '' NOT NULL
);

CREATE INDEX userblacklist_userblacklist_idx ON userblacklist (username, domain, prefix);

INSERT INTO version (table_name, table_version) values ('globalblacklist','1');
CREATE TABLE globalblacklist (
    id SERIAL PRIMARY KEY NOT NULL,
    prefix VARCHAR(64) DEFAULT '' NOT NULL,
    whitelist SMALLINT DEFAULT 0 NOT NULL,
    description VARCHAR(64) DEFAULT '' NOT NULL
);

CREATE INDEX globalblacklist_globalblacklist_idx ON globalblacklist (prefix);

