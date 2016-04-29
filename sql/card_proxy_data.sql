
/*

data for bootstrap:

kek1 = 'dd045814b7272e9b1a8b7f22c512496741d4633aacf17992107f5977e5430fb6'
kek2 = '30028956eb15f46821109d8bdea6f75833ce82893efa8d1f0b84d816590e2406'
kek3 = '8eb27d13044a190297b7adddfcd5a915d81ecb00a2bec53bcbfe6fab156e5e50'
kek0 = '%032x' % (int(kek1, 16) ^ int(kek2, 16) ^ int(kek3, 16))
# kek0 = '63b4ac515878c3f1ac2c4f74e761172aaa042ab330b531b6d005eecaa92375e0'
import hashlib
kek = hashlib.sha256(kek0.decode('hex')).hexdigest()
# kek = '47c91030f98850ff8319504595fbf08eae5000301018c1b8208140133ef1b08e'
import aes_crypter
cr = aes_crypter.AESCrypter(kek.decode('hex'))
hmac = '59e5787e9b046497b7df37d8519298e3212e70f2fb65d56510ee0604f38348c5'
cr.encrypt('Lorem ipsum dolo').encode('base64').strip()
# 'H8FTG4XiFd9GODFyMFj13g=='
cr.encrypt(hmac.decode('hex')).encode('base64').strip()
# 'jQXgMdVOeLLIkf+eNLUzbkmeN7rol4iRYpHJRRkJJg4='

*/

insert into t_config(ckey, cvalue) values('STATE', 'NORMAL');

insert into t_config(ckey, cvalue) values('KEK_CONTROL_PHRASE', 'Lorem ipsum dolo');

insert into t_config(ckey, cvalue) values('KEK_VERSION', '0');

insert into t_config(ckey, cvalue) values('KEK_VER0_PART3', '8eb27d13044a190297b7adddfcd5a915d81ecb00a2bec53bcbfe6fab156e5e50');

insert into t_config(ckey, cvalue) values('KEK_VER0_PART1_CHECK', '1');
insert into t_config(ckey, cvalue) values('KEK_VER0_PART2_CHECK', '1');
insert into t_config(ckey, cvalue) values('KEK_VER0_PART3_CHECK', '1');

insert into t_config(ckey, cvalue) values('KEK_VER0_CONTROL_CODE', 'H8FTG4XiFd9GODFyMFj13g==');

insert into t_config(ckey, cvalue) values('HMAC_VERSION', '0');

insert into t_dek(start_ts, finish_ts, dek_crypted, kek_version, max_counter, counter)
    values (current_timestamp, date_add(current_timestamp, interval 2 year), 'jQXgMdVOeLLIkf+eNLUzbkmeN7rol4iRYpHJRRkJJg4=', 0, 0, 0);

insert into t_config(ckey, cvalue) values('HMAC_VER0_ID', last_insert_id());

/* test users: password=qwerty */

insert into t_vault_user(update_ts, login, passwd, roles) values (current_timestamp(), 'tokenize_only', '65e84be33532fb784c48129675f9eff3a682b27168c0ea744b2cf58ee02337c5', '[tokenize:mydomain]');

insert into t_vault_user(update_ts, login, passwd, roles) values (current_timestamp(), 'detokenize_only', '65e84be33532fb784c48129675f9eff3a682b27168c0ea744b2cf58ee02337c5', '[detokenize:mydomain]');

commit;

