
GRANT SELECT ON card_proxy.t_config TO cpr_service@'%';
GRANT SELECT ON card_proxy.t_dek TO cpr_service@'%';
GRANT SELECT, DELETE ON card_proxy.t_data_token TO cpr_service@'%';

GRANT SELECT, INSERT, UPDATE, DELETE ON card_proxy.t_config TO cpr_keyapi@'%';
GRANT SELECT, INSERT, UPDATE, DELETE ON card_proxy.t_dek TO cpr_keyapi@'%';
GRANT SELECT, UPDATE ON card_proxy.t_data_token TO cpr_keyapi@'%';

GRANT SELECT ON card_proxy.t_config TO cpr_tokenizer@'%';
GRANT SELECT, INSERT, UPDATE ON card_proxy.t_dek TO cpr_tokenizer@'%';
GRANT SELECT, INSERT, DELETE ON card_proxy.t_data_token TO cpr_tokenizer@'%';

GRANT SELECT ON card_proxy.t_config TO cpr_secvalut@'%';
GRANT SELECT, INSERT, UPDATE ON card_proxy.t_dek TO cpr_secvalut@'%';
GRANT SELECT, INSERT, DELETE ON card_proxy.t_secure_vault TO cpr_secvalut@'%';

FLUSH PRIVILEGES;
