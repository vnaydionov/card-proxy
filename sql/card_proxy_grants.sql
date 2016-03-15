
GRANT SELECT, INSERT, UPDATE, DELETE ON card_proxy.* TO cpr_service@'%';

GRANT SELECT ON card_proxy.* TO cpr_keyapi@'%';
GRANT INSERT, UPDATE, DELETE ON card_proxy.t_config TO cpr_keyapi@'%';
GRANT UPDATE ON card_proxy.t_dek TO cpr_keyapi@'%';
GRANT UPDATE ON card_proxy.t_data_token TO cpr_keyapi@'%';

GRANT SELECT ON card_proxy.* TO cpr_tokenizer@'%';
GRANT INSERT, UPDATE, DELETE ON card_proxy.t_data_token TO cpr_tokenizer@'%';
GRANT INSERT, UPDATE, DELETE ON card_proxy.t_dek TO cpr_tokenizer@'%';

FLUSH PRIVILEGES;
