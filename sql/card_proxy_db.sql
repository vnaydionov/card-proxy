
CREATE DATABASE card_proxy DEFAULT CHARSET utf8;

CREATE USER cpr_service@'%' IDENTIFIED BY 'cpr_service_pwd';
CREATE USER cpr_keyapi@'%' IDENTIFIED BY 'cpr_keyapi_pwd';
CREATE USER cpr_tokenizer@'%' IDENTIFIED BY 'cpr_tokenizer_pwd';

FLUSH PRIVILEGES;

