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
    dek_crypted VARCHAR(94) NOT NULL,
    kek_version INT NOT NULL,
    max_counter BIGINT NOT NULL DEFAULT 0,
    counter BIGINT NOT NULL DEFAULT 0
    , PRIMARY KEY (id)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

ALTER TABLE t_data_token ADD FOREIGN KEY (dek_id) REFERENCES t_dek(id);

CREATE INDEX i_dtoken_finish ON t_data_token(finish_ts);

CREATE UNIQUE INDEX i_dtoken_string ON t_data_token(token_string);

CREATE INDEX i_dtoken_dek ON t_data_token(dek_id);

CREATE INDEX i_dtoken_hmac ON t_data_token(hmac_digest);

CREATE INDEX i_dtoken_hmac_ver ON t_data_token(hmac_version);

CREATE INDEX i_dek_finish ON t_dek(finish_ts);

CREATE INDEX i_dek_kek_ver ON t_dek(kek_version);

