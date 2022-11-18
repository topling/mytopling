#!/bin/sh

echo ------------------------------------------------------------------------
echo test fkfv: fixed key fixed value...
mysql -D test -u root -h 127.0.0.1 --protocol=tcp << EOF
drop table if exists fkfv;
create table fkfv(pk int primary key, sk varchar(16))
engine = 'rocksdb' default charset binary;
insert into fkfv(pk, sk) values
(1, '-aaaa'),
(2, '-bbbb'),
(3, '-cccc'),
(4, '-dddd'),
(5, '-eeee');
create index fkfv_sk on fkfv(sk);
select * from fkfv where sk = '-bbbb';
EOF
echo test fkfv: fixed key fixed value... done

echo ------------------------------------------------------------------------
echo test fkvv: fixed key varlen value...
mysql -D test -u root -h 127.0.0.1 --protocol=tcp << EOF
drop table if exists fkvv;
create table fkvv(pk varchar(32) primary key, sk char(16))
engine = 'rocksdb' default charset latin1;
insert into fkvv(pk, sk) values
('a'    , '--aaaa'),
('bb'   , '--bbbb'),
('ccc'  , '--cccc'),
('dddd' , '--dddd'),
('eeeee', '--eeee');
create index fkvv_sk on fkvv(sk);
select * from fkvv where sk = '--bbbb';
EOF
echo test fkvv: fixed key varlen value... done

echo ------------------------------------------------------------------------
echo test vkfv: varlen key fixed value...
mysql -D test -u root -h 127.0.0.1 --protocol=tcp << EOF
drop table if exists vkfv;
create table vkfv(pk int primary key, sk varchar(32))
engine = 'rocksdb' default charset binary;
insert into vkfv(pk, sk) values
(1, '---aaaa'),
(2, '---bbbb_bbbb'),
(3, '---cccc_cccc_cccc'),
(4, '---dddd_dddd_dddd_dddd'),
(5, '---eeee_eeee_eeee_eeee_eeee');
create index vkfv_sk on vkfv(sk);
select * from vkfv where sk = '---bbbb_bbbb';
EOF
echo test vkfv: varlen key fixed value... done

echo ------------------------------------------------------------------------
echo test vkvv: varlen key varlen value...
mysql -D test -u root -h 127.0.0.1 --protocol=tcp << EOF
drop table if exists vkvv;
create table vkvv(pk varchar(64) primary key,
                  sk varchar(64))
    engine = 'rocksdb' default charset latin1;
insert into vkvv(pk, sk) values
('1111'                         , '----aaaa'),
('2222_2222'                    , '----bbbb_bbbb'),
('3333_3333_3333'               , '----cccc_cccc_cccc'),
('4444_4444_4444_4444'          , '----dddd_dddd_dddd_dddd'),
('55555_55555_55555_55555_55555', '----eeeee_eeeee_eeeee_eeeee_eeeee');
create index vkvv_sk on vkvv(sk);
select * from vkvv where sk = '----bbbb_bbbb';
EOF
echo test vkvv: varlen key fixed value... done
echo ------------------------------------------------------------------------
