
insert into t_config(ckey, cvalue) values('KEK_VERSION', '0');

insert into t_config(ckey, cvalue) values('KEK_VER0_PART3', '7491ad08dc2156a7a08e768074f9504226e21ab933ec5f1d28833f0be1d3ef72');

insert into t_config(ckey, cvalue) values('HMAC_VERSION', '0');

insert into t_config(ckey, cvalue) values('HMAC_VER0_ID', '1');

insert into t_dek(start_ts, finish_ts, dek_crypted, kek_version, max_counter, counter) 
    values (current_timestamp, date_add(current_timestamp, interval 2 year), 'OzAkYQKmi3BNZgliTThgSsJp6yDu/w3Tl2ddb2tu/JY=', 0, 10, 0);

commit;
