-- DBTYPE=MYSQL

CREATE TABLE t_config (
    ckey VARCHAR(80) NOT NULL,
    cvalue VARCHAR(1000) NULL,
    update_ts TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP
    , PRIMARY KEY (ckey)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

CREATE TABLE t_data_token (
    id BIGINT NOT NULL AUTO_INCREMENT,
    finish_ts DATETIME NOT NULL,
    token_string VARCHAR(32) NOT NULL,
    data_crypted VARCHAR(25) NOT NULL,
    dek_id BIGINT NOT NULL,
    hmac_digest VARCHAR(46) NOT NULL,
    hmac_version INT NOT NULL
    , PRIMARY KEY (id)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

CREATE TABLE t_dek (
    id BIGINT NOT NULL AUTO_INCREMENT,
    start_ts DATETIME NOT NULL,
    finish_ts DATETIME NOT NULL,
    dek_crypted VARCHAR(46) NOT NULL,
    kek_version INT NOT NULL,
    max_counter BIGINT NOT NULL DEFAULT 0,
    counter BIGINT NOT NULL DEFAULT 0
    , PRIMARY KEY (id)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

CREATE TABLE t_secure_vault (
    id BIGINT NOT NULL AUTO_INCREMENT,
    create_ts DATETIME NOT NULL,
    finish_ts DATETIME NOT NULL,
    token_string VARCHAR(32) NOT NULL,
    data_crypted VARCHAR(9500) NOT NULL,
    dek_id BIGINT NOT NULL,
    hmac_digest VARCHAR(46) NOT NULL,
    hmac_version INT NOT NULL,
    creator_id BIGINT NULL,
    domain VARCHAR(30) NULL,
    comment VARCHAR(250) NULL,
    notify_email VARCHAR(250) NULL,
    notify_status VARCHAR(250) NULL
    , PRIMARY KEY (id)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

CREATE TABLE t_vault_user (
    id BIGINT NOT NULL AUTO_INCREMENT,
    update_ts DATETIME NOT NULL,
    login VARCHAR(50) NOT NULL,
    passwd VARCHAR(100) NULL,
    roles VARCHAR(250) NULL
    , PRIMARY KEY (id)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

ALTER TABLE t_data_token ADD FOREIGN KEY (dek_id) REFERENCES t_dek(id);

ALTER TABLE t_secure_vault ADD FOREIGN KEY (dek_id) REFERENCES t_dek(id);

ALTER TABLE t_secure_vault ADD FOREIGN KEY (creator_id) REFERENCES t_vault_user(id);

CREATE INDEX i_dtoken_finish ON t_data_token(finish_ts);

CREATE UNIQUE INDEX i_dtoken_string ON t_data_token(token_string);

CREATE INDEX i_dtoken_dek ON t_data_token(dek_id);

CREATE INDEX i_dtoken_hmac ON t_data_token(hmac_digest);

CREATE INDEX i_dtoken_hmac_ver ON t_data_token(hmac_version);

CREATE INDEX i_dek_finish ON t_dek(finish_ts);

CREATE INDEX i_dek_kek_ver ON t_dek(kek_version);

CREATE INDEX i_secvault_finish ON t_secure_vault(finish_ts);

CREATE UNIQUE INDEX i_secvault_string ON t_secure_vault(token_string);

CREATE INDEX i_secvault_dek ON t_secure_vault(dek_id);

CREATE INDEX i_secvault_hmac ON t_secure_vault(hmac_digest);

CREATE INDEX i_secvault_hmac_ver ON t_secure_vault(hmac_version);

CREATE INDEX i_secvault_creator ON t_secure_vault(creator_id);

CREATE INDEX i_secvault_domain ON t_secure_vault(domain);

CREATE INDEX i_vault_user_login ON t_vault_user(login);

