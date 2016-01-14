-- DBTYPE=MYSQL

CREATE TABLE t_card (
    id BIGINT NOT NULL AUTO_INCREMENT,
    dek_id BIGINT NOT NULL,
    card_token VARCHAR(32) NOT NULL,
    ts TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    store_ts TIMESTAMP NOT NULL,
    pan_crypted VARCHAR(25) NOT NULL,
    expire_year INT NOT NULL,
    expire_month INT NOT NULL,
    card_holder VARCHAR(100) NOT NULL,
    pan_masked VARCHAR(20) NOT NULL
    , PRIMARY KEY (id)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

CREATE TABLE t_config (
    ckey VARCHAR(40) NOT NULL,
    cvalue VARCHAR(100) NULL,
    update_ts TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP
    , PRIMARY KEY (ckey)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

CREATE TABLE t_dek (
    id BIGINT NOT NULL AUTO_INCREMENT,
    dek_crypted VARCHAR(100) NOT NULL,
    start_ts TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    finish_ts TIMESTAMP NULL,
    counter BIGINT NOT NULL DEFAULT 0,
    old_kek INT NOT NULL DEFAULT 0
    , PRIMARY KEY (id)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

CREATE TABLE t_incoming_request (
    id BIGINT NOT NULL AUTO_INCREMENT,
    dek_id BIGINT NOT NULL,
    ts TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    cvn_token VARCHAR(32) NOT NULL,
    cvn_crypted VARCHAR(25) NOT NULL
    , PRIMARY KEY (id)
) ENGINE=INNODB DEFAULT CHARSET=utf8;

ALTER TABLE t_card ADD FOREIGN KEY (dek_id) REFERENCES t_dek(id);

ALTER TABLE t_incoming_request ADD FOREIGN KEY (dek_id) REFERENCES t_dek(id);

-- manually added:

create index i_card_store on t_card(store_ts);
create index i_card_dedup on t_card(pan_masked, expire_year, expire_month);
create index i_dek_finish_ts on t_dek(finish_ts);

create unique index i_card_token on t_card(card_token);
create unique index i_cvn_token on t_incoming_request(cvn_token);

