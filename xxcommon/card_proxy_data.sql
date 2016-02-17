
/*
data for bootstrap:

KEK1: 'dd045814b7272e9b1a8b7f22c512496741d4633aacf17992107f5977e5430fb6'
KEK2: '30028956eb15f46821109d8bdea6f75833ce82893efa8d1f0b84d816590e2406'
KEK3: '8eb27d13044a190297b7adddfcd5a915d81ecb00a2bec53bcbfe6fab156e5e50'

KEK0: '63b4ac515878c3f1ac2c4f74e761172aaa042ab330b531b6d005eecaa92375e0'
KEK:  '47c91030f98850ff8319504595fbf08eae5000301018c1b8208140133ef1b08e'

HMAC: '59e5787e9b046497b7df37d8519298e3212e70f2fb65d56510ee0604f38348c5'
*/

insert into t_config(ckey, cvalue) values('KEK_VERSION', '0');

insert into t_config(ckey, cvalue) values('KEK_VER0_PART3', '8eb27d13044a190297b7adddfcd5a915d81ecb00a2bec53bcbfe6fab156e5e50');

insert into t_config(ckey, cvalue) values('KEK_CONTROL_PHRASE', 'Lorem ipsum dolo');

insert into t_config(ckey, cvalue) values('HMAC_VERSION', '0');

insert into t_config(ckey, cvalue) values('HMAC_VER0_ID', '1');

insert into t_dek(start_ts, finish_ts, dek_crypted, kek_version, max_counter, counter) 
    values (current_timestamp, date_add(current_timestamp, interval 2 year), 'jQXgMdVOeLLIkf+eNLUzbkmeN7rol4iRYpHJRRkJJg4=', 0, 0, 0);

commit;
