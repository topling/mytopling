drop table if exists A;
create table A (
	a00 bigint not null,
	a01 bigint not null,
	a02 bigint not null,
	a03 bigint not null,
	a04 bigint not null,
	a05 bigint not null,
	a06 bigint not null,
	a07 bigint not null,
	a08 bigint not null,
	a09 bigint not null,
	a10 bigint not null,
	a11 bigint not null,
	a12 bigint not null,
	a13 bigint not null,
	a14 bigint not null,
	a15 bigint not null,
	primary key (a00, a01, a02, a03, a04, a05, a06, a07, a08, a09,a10, a11, a12, a13, a14, a15)
);

drop procedure if exists insert_A;
DELIMITER //
CREATE PROCEDURE insert_A()
BEGIN
    DECLARE i bigint DEFAULT 1;
    WHILE i <= 100000 DO
	INSERT INTO A(a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, a10, a11, a12, a13, a14, a15)
	VALUES(i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i);
	SET i = i + 1;
    END WHILE;
END //
DELIMITER ;


call insert_A();
