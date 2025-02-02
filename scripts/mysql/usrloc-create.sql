INSERT INTO version (table_name, table_version) values ('location','1006');
CREATE TABLE location (
    id INT(10) UNSIGNED AUTO_INCREMENT PRIMARY KEY NOT NULL,
    username CHAR(64) DEFAULT '' NOT NULL,
    domain CHAR(64) DEFAULT '' NOT NULL,
    contact CHAR(255) DEFAULT '' NOT NULL,
    received CHAR(128) DEFAULT NULL,
    path CHAR(128) DEFAULT NULL,
    expires DATETIME DEFAULT '2020-05-28 21:32:15' NOT NULL,
    q FLOAT(10,2) DEFAULT 1.0 NOT NULL,
    callid CHAR(255) DEFAULT 'Default-Call-ID' NOT NULL,
    cseq INT(11) DEFAULT 13 NOT NULL,
    last_modified DATETIME DEFAULT '1900-01-01 00:00:01' NOT NULL,
    flags INT(11) DEFAULT 0 NOT NULL,
    cflags INT(11) DEFAULT 0 NOT NULL,
    user_agent CHAR(255) DEFAULT '' NOT NULL,
    socket CHAR(64) DEFAULT NULL,
    methods INT(11) DEFAULT NULL,
    CONSTRAINT account_contact_idx UNIQUE (username, domain, contact, callid)
) ENGINE=MyISAM;

