#
# wl#8919  Implement NOWAIT and SKIP LOCKED
#

# needed by start_transaction_high_prio.inc
--source include/have_debug.inc

--source include/count_sessions.inc

connect (con1,localhost,root,,);
if ($engine=="INNODB")
{
  SET SESSION innodb_lock_wait_timeout=1;
}
if ($engine=="ROCKSDB")
{
  SET SESSION rocksdb_lock_wait_timeout=1;
}

connection default;
if ($engine=="INNODB")
{
  SET SESSION innodb_lock_wait_timeout=1;
}
if ($engine=="ROCKSDB")
{
  SET SESSION rocksdb_lock_wait_timeout=1;
}

--echo # Case 1: Test primary index
eval CREATE TABLE t1(
        seat_id    INT,
        state      INT,
        PRIMARY KEY(seat_id)
) ENGINE=$engine;

INSERT INTO t1 VALUES(1,0), (2,0), (3,0), (4,0);

BEGIN;
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR SHARE;

connection con1;
BEGIN;

SELECT * FROM t1 WHERE state = 0 LIMIT 2 LOCK IN SHARE MODE;
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR SHARE;

SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR SHARE NOWAIT;
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR SHARE SKIP LOCKED;

--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR UPDATE;

--error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR UPDATE NOWAIT;
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR UPDATE SKIP LOCKED;

--error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE seat_id > 0 LIMIT 2 FOR UPDATE NOWAIT;
SELECT * FROM t1 WHERE seat_id > 0 LIMIT 2 FOR UPDATE SKIP LOCKED;

COMMIT;

connection default;
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR UPDATE;

connection con1;
--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 LIMIT 2 LOCK IN SHARE MODE;

--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR SHARE;

--error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR SHARE NOWAIT;
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR SHARE SKIP LOCKED;

--error ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR UPDATE;

--error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR UPDATE NOWAIT;
SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR UPDATE SKIP LOCKED;

--error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE seat_id > 0 LIMIT 2 FOR UPDATE NOWAIT;
SELECT * FROM t1 WHERE seat_id > 0 LIMIT 2 FOR UPDATE SKIP LOCKED;

COMMIT;

connection default;
COMMIT;

DROP TABLE t1;

--echo # Case 2: Test primary index & secondary index
eval CREATE TABLE t1(
        seat_id    INT,
        row_id     INT,
        state      INT,
        PRIMARY KEY(seat_id),
        KEY(row_id)
) ENGINE=$engine;

INSERT INTO t1 VALUES(1,1,0), (2,1,0), (3,2,0), (4,2,0);

--echo # Test secondary key
# Case 2a: secondary blocks secondary/primary
BEGIN;
SELECT * FROM t1 WHERE state = 0 AND row_id = 1 LIMIT 1 FOR UPDATE NOWAIT;

connection con1;
BEGIN;
--error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 AND row_id = 1 LIMIT 1 FOR UPDATE NOWAIT;
SELECT * FROM t1 WHERE state = 0 AND row_id = 1 LIMIT 1 FOR UPDATE SKIP LOCKED;

--error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 AND row_id > 0 LIMIT 1 FOR UPDATE NOWAIT;
SELECT * FROM t1 WHERE state = 0 AND row_id > 0 LIMIT 1 FOR UPDATE SKIP LOCKED;

--error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 FOR UPDATE NOWAIT;
SELECT * FROM t1 WHERE state = 0 FOR UPDATE SKIP LOCKED;

COMMIT;

connection default;
COMMIT;

# Case 2b: primary blocks secondary/primary
BEGIN;
SELECT * FROM t1 WHERE seat_id = 1 FOR UPDATE NOWAIT;

connection con1;
BEGIN;
--error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 AND row_id = 1 LIMIT 1 FOR UPDATE NOWAIT;
SELECT * FROM t1 WHERE state = 0 AND row_id = 1 LIMIT 1 FOR UPDATE SKIP LOCKED;

--error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
SELECT * FROM t1 WHERE state = 0 FOR UPDATE NOWAIT;
SELECT * FROM t1 WHERE state = 0 FOR UPDATE SKIP LOCKED;

COMMIT;

connection default;
COMMIT;

DROP TABLE t1;

if ($engine=='INNODB')
{
  --echo # Case 3: Test primary index & spatial index
  eval CREATE TABLE t1(
          seat_id    INT,
          pos        POINT NOT NULL SRID 0,
          state      INT,
          PRIMARY KEY(seat_id),
          SPATIAL KEY(pos)
  ) ENGINE=$engine;

  INSERT INTO t1 VALUES
  (1,ST_PointFromText('POINT(1 0)'),0),
  (2,ST_PointFromText('POINT(1 1)'),0),
  (3,ST_PointFromText('POINT(2 0)'),0),
  (4,ST_PointFromText('POINT(2 1)'),0),
  (5,ST_PointFromText('POINT(3 0)'),0),
  (6,ST_PointFromText('POINT(3 1)'),0);

  # Case 3a: secondary blocks secondary/primary
  BEGIN;
  SET @g = ST_GeomFromText('POLYGON((0 0,0 2,2 2,0 2,0 0))');
  # the first 4 records in the rtree index page are locked
  SELECT seat_id, state, ST_AsText(pos) FROM t1 FORCE INDEX (pos)
  WHERE state = 0 AND MBRWithin(pos, @g) FOR UPDATE NOWAIT;

  connection con1;
  BEGIN;
  SET @g = ST_GeomFromText('POLYGON((0 0,0 4,4 4,0 4,0 0))');
  --error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
  SELECT seat_id, state, ST_AsText(pos) FROM t1 FORCE INDEX (pos)
  WHERE state = 0 AND MBRWithin(pos, @g) FOR UPDATE NOWAIT;

  SELECT seat_id, state, ST_AsText(pos) FROM t1 FORCE INDEX (pos)
  WHERE state = 0 AND MBRWithin(pos, @g)  FOR UPDATE SKIP LOCKED;

  --error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
  SELECT seat_id, state, ST_AsText(pos) FROM t1
  WHERE state = 0 FOR UPDATE NOWAIT;

  SELECT seat_id, state, ST_AsText(pos) FROM t1
  WHERE state = 0 FOR UPDATE SKIP LOCKED;

  COMMIT;

  connection default;
  COMMIT;

  # Case 3b: primary blockes secondary/primary
  connection con1;
  SET @g = ST_GeomFromText('POLYGON((0 0,0 3,3 3,0 3,0 0))');
  SELECT seat_id, state, ST_AsText(pos) FROM t1 FORCE INDEX (pos)
  WHERE state = 0 AND MBRWithin(pos, @g) FOR UPDATE;

  connection default;
  BEGIN;
  SELECT seat_id, state, ST_AsText(pos) FROM t1
  WHERE seat_id = 4 FOR UPDATE NOWAIT;

  connection con1;
  --error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
  SELECT seat_id, state, ST_AsText(pos) FROM t1 FORCE INDEX (pos)
  WHERE state = 0 AND MBRWithin(pos, @g) FOR UPDATE NOWAIT;

  SELECT seat_id, state, ST_AsText(pos) FROM t1 FORCE INDEX (pos)
  WHERE state = 0 AND MBRWithin(pos, @g) FOR UPDATE SKIP LOCKED;

  --error ER_LOCK_NOWAIT, ER_LOCK_WAIT_TIMEOUT
  SELECT seat_id, state, ST_AsText(pos) FROM t1
  WHERE state = 0 FOR UPDATE NOWAIT;

  SELECT seat_id, state, ST_AsText(pos) FROM t1
  WHERE state = 0 FOR UPDATE SKIP LOCKED;

  connection default;
  COMMIT;

  DROP TABLE t1;

  --echo # Case 4: Test high priority transaction
  eval CREATE TABLE t1(
          seat_id    INT,
          state      INT,
          PRIMARY KEY(seat_id)
  ) ENGINE=$engine;

  INSERT INTO t1 VALUES(1,0), (2,0), (3,0), (4,0);

  BEGIN;
  SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR UPDATE;

  connection con1;

  --source include/start_transaction_high_prio.inc

  SELECT * FROM t1 WHERE seat_id = 1 AND state = 0 FOR UPDATE NOWAIT;

  SELECT * FROM t1 WHERE state = 0 LIMIT 2 FOR UPDATE SKIP LOCKED;

  COMMIT;

  connection default;
  --error ER_ERROR_DURING_COMMIT
  COMMIT;
  ROLLBACK;

  DROP TABLE t1;
}

disconnect con1;

--source include/wait_until_count_sessions.inc
