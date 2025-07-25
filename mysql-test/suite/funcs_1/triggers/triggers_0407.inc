#======================================================================
#
# Trigger Tests
# (test case numbering refer to requirement document TP v1.1)
#======================================================================
set GLOBAL sql_mode="";

USE test;
--source suite/funcs_1/include/tb3.inc

--disable_abort_on_error

# General setup for Trigger tests
let $message= Testcase: 3.5:;
--source include/show_msg.inc

--disable_abort_on_error

	create User test_general@localhost;
	set password for test_general@localhost = password('PWD');
	revoke ALL PRIVILEGES, GRANT OPTION FROM test_general@localhost;

	create User test_super@localhost;
	set password for test_super@localhost = password('PWD');
	grant ALL on *.* to test_super@localhost with grant OPTION;
	connect (con1_general,localhost,test_general,PWD,"*NO-ONE*",$MASTER_MYPORT,$MASTER_MYSOCK);
# next is to check that we connected above
        connection con1_general;
	connect (con1_super,localhost,test_super,PWD,"*NO-ONE*",$MASTER_MYPORT,$MASTER_MYSOCK);
# next is to check that we connected above
        connection con1_super;
	connection default;

####################################
############ Section 3.5.4 #########
# Drop Trigger Checkes:            #
####################################
let $message= Testcase 3.5.4:;
--source include/show_msg.inc

	connection default;
	use test;

#Section 3.5.4.1
# Testcase: Ensure that the DROP TRIGGER statement cleanly drops its target trigger.
let $message= Testcase 3.5.4.1:;
--source include/show_msg.inc

	connection con1_super;
	create database db_drop;
	Use db_drop;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 char(30)) engine = $engine_type;
	grant INSERT, SELECT on db_drop.t1 to test_general;
	Use db_drop;
	Create trigger trg1 BEFORE INSERT on t1
		for each row set new.f1='Trigger 3.5.4.1';
	connection con1_general;
	Use db_drop;
	Insert into t1 values ('Insert error 3.5.4.1');
	Select * from t1 order by f1;
	connection con1_super;
	drop trigger trg1;
	select trigger_schema, trigger_name, event_object_table
	from information_schema.triggers
        where trigger_schema = 'db_drop'
        order by trigger_name;
	connection con1_general;
	Insert into t1 values ('Insert no trigger 3.5.4.1');
	Select * from t1 order by f1;

#Cleanup
	--disable_warnings
	connection con1_super;
        --disable_warnings
	--error 0,ER_TRG_DOES_NOT_EXIST
	drop trigger trg1;
	drop database if exists db_drop;
	revoke ALL PRIVILEGES, GRANT OPTION FROM 'test_general'@'localhost';
	--enable_warnings

#Section 3.5.4.2
# Test case: Ensure that DROP TRIGGER <trigger name> fails, with an appropriate error
#            message, if the trigger name does not exist.
let $message= Testcase 3.5.4.2:;
--source include/show_msg.inc

	connection con1_super;
	create database db_drop2;
	Use db_drop2;
	--disable_warnings
	drop table if exists t1_432 ;
	--enable_warnings
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1_432 (f1 char (30)) engine = $engine_type;
	--error ER_TRG_DOES_NOT_EXIST
	Drop trigger tr_does_not_exit;
#cleanup
	--disable_warnings
	drop table if exists t1_432 ;
	drop database  if exists db_drop2;
	--enable_warnings

#Section 3.5.4.3
# Test case: Ensure that DROP TRIGGER <trigger name> fails, with an appropriate
#            error message, if <trigger name> is not a qualified name.
let $message= Testcase 3.5.4.3:;
--source include/show_msg.inc

	connection con1_super;
	create database db_drop3;
	Use db_drop3;
	--disable_warnings
	drop table if exists t1_433 ;
	drop table if exists t1_433a ;
	--enable_warnings
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1_433 (f1 char (30)) engine = $engine_type;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1_433a (f1a char (5)) engine = $engine_type;

	CREATE TRIGGER trg3 BEFORE INSERT on t1_433 for each row
		set new.f1 = 'Trigger 3.5.4.3';

# Using table
	--error ER_PARSE_ERROR
	Drop trigger t1.433.trg3;

# Using database.table
	--error ER_PARSE_ERROR
	Drop trigger db_drop3.t1.433.trg3;

# wrong database
	--error ER_TRG_DOES_NOT_EXIST
	Drop trigger mysql.trg3;

# database does not exist
	--error ER_TRG_DOES_NOT_EXIST
	Drop trigger tbx.trg3;

#cleanup
	Drop trigger db_drop3.trg3;
	drop table if exists t1_433;
	drop table if exists t1_433a;
	drop database if exists db_drop3;

#Section 3.5.4.4
# Test case: Ensure that when a database is dropped, all triggers created within
#            that database are also cleanly dropped.
let $message= Testcase 3.5.4.4:;
--source include/show_msg.inc

	connection con1_super;
	create database db_drop4;
	Use db_drop4;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 char(30)) engine = $engine_type;
	grant INSERT, SELECT on db_drop4.t1 to test_general;
	Create trigger trg4 BEFORE INSERT on t1
		for each row set new.f1='Trigger 3.5.4.4';
	connection con1_general;
	Use db_drop4;
	Insert into t1 values ('Insert 3.5.4.4');
	Select * from t1;
	connection con1_super;
	Drop database db_drop4;
	Show databases like 'db_drop4';
	select trigger_schema, trigger_name, event_object_table
		from information_schema.triggers
		where information_schema.triggers.trigger_name='trg4';
	create database db_drop4;
	Use db_drop4;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 char(30)) engine = $engine_type;
	grant INSERT, SELECT on db_drop4.t1 to test_general;
	connection con1_general;
	Insert into t1 values ('2nd Insert 3.5.4.4');
	Select * from t1;

#Cleanup
	connection con1_super;
        --disable_warnings
	--error ER_TRG_DOES_NOT_EXIST
	drop trigger trg4;
	drop database if exists db_drop4;
	--enable_warnings
	revoke ALL PRIVILEGES, GRANT OPTION FROM 'test_general'@'localhost';

#Section 3.5.4.5
# Test case: Ensure that when a table is dropped, all triggers for which it is the
#            subject table are also cleanly dropped.
let $message= Testcase 3.5.4.5:;
--source include/show_msg.inc

	connection con1_super;
	create database db_drop5;
	Use db_drop5;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 char(50)) engine = $engine_type;
	grant INSERT, SELECT on t1 to test_general;
	Create trigger trg5 BEFORE INSERT on t1
		for each row set new.f1='Trigger 3.5.4.5';
	connection con1_general;
	Use db_drop5;
	Insert into t1 values ('Insert 3.5.4.5');
	Select * from t1;
	connection con1_super;
	Drop table t1;
	Show tables;
	select trigger_schema, trigger_name, event_object_table
		from information_schema.triggers
		where information_schema.triggers.trigger_name='trg5';
        --replace_result $engine_type <engine_to_be_used>
	eval create table t1 (f1 char(50)) engine = $engine_type;
	grant INSERT, SELECT on t1 to test_general;
	connection con1_general;
	Insert into t1 values ('2nd Insert 3.5.4.5');
	Select * from t1;

#Cleanup
	connection con1_super;
        --disable_warnings
	--error ER_TRG_DOES_NOT_EXIST
	drop trigger trg5;
	drop database if exists db_drop5;
	--enable_warnings
	revoke ALL PRIVILEGES, GRANT OPTION FROM 'test_general'@'localhost';


##################################
######### Section 3.5.5 ##########
# Checks on the Subject Table    #
##################################

let $message= Testcase 3.5.5:;
--source include/show_msg.inc

	connection default;
	use test;

#Section 3.5.5.1
# Test case: Ensure that, if CREATE TRIGGER is executed with a non-existent
#            subject table, the statement fails with an appropriate error message.
let $message= Testcase 3.5.5.1:;
--source include/show_msg.inc

	--error ER_NO_SUCH_TABLE
	Create trigger trg1 before INSERT on t100 for each row set new.f2=1000;


#Section 3.5.5.2
# Test case: Ensure that, if CREATE TRIGGER is executed with a temporary table
#           as the subject table, the statement fails with an appropriate error message.
let $message= Testcase 3.5.5.2:;
--source include/show_msg.inc
	
	Create temporary table t1_temp (f1 bigint signed, f2 bigint unsigned);
	
	--error ER_TRG_ON_VIEW_OR_TEMP_TABLE
	Create trigger trg2 before INSERT
		on t1_temp for each row set new.f2=9999;

#Cleanup
	--disable_warnings
	drop table t1_temp;
	--enable_warnings


#Section 3.5.5.3
# Test case: Ensure that, if CREATE TRIGGER is executed with a view as the subject
#            table, the statement fails with an appropriate error message.
let $message= Testcase 3.5.5.3:;
--source include/show_msg.inc

	Create view vw3 as select f118 from tb3;
	
# OBN Not sure why the server is returning error ER_WRONG_OBJECT
	--error ER_WRONG_OBJECT
	Create trigger trg3 before INSERT
		on vw3 for each row set new.f118='s';

#Cleanup
	--disable_warnings
	drop view vw3;
	--enable_warnings


#Section 3.5.5.4
# Test case: Ensure that, if CREATE TRIGGER is executed with a table that resides
#            in a different database than in which the trigger will reside, the
#            statement fails with an appropriate error message; that is, ensure that
#            the trigger and its subject table must reside in the same database.
let $message= Testcase 3.5.5.4:;
--source include/show_msg.inc

	connection con1_super;
	create database dbtest_one;
	create database dbtest_two;
	use dbtest_two;
        --replace_result $engine_type <engine_to_be_used>
	eval create table t2 (f1 char(15)) engine = $engine_type;
	use dbtest_one;
	--error ER_TRG_IN_WRONG_SCHEMA
	create trigger trg4 before INSERT
		on dbtest_two.t2 for each row set new.f1='trig 3.5.5.4';
	grant INSERT, SELECT on dbtest_two.t2 to test_general;
	grant SELECT on dbtest_one.* to test_general;
	connection con1_general;
	use dbtest_two;
	Insert into t2 values ('1st Insert 3.5.5.4');
	Select * from t2;
	use dbtest_one;
	Insert into dbtest_two.t2 values ('2nd Insert 3.5.5.4');
	Select * from dbtest_two.t2 order by f1;

#Cleanup
	connection con1_super;
	--disable_warnings
	revoke ALL PRIVILEGES, GRANT OPTION FROM 'test_general'@'localhost';
	DROP DATABASE if exists dbtest_one;
	drop database if EXISTS dbtest_two;
	--enable_warnings

#####################################
########### Section 3.5.6 ###########
# Check on the Trigger Action Time  #
#####################################

let $message= Testcase 3.5.6:;
--source include/show_msg.inc

	connection default;
	use test;

#Section 3.5.6.1
# Test case: Ensure that a trigger definition can specify a trigger action time of BEFORE.
# See section 3.5.1.1
let $message= Testcase 3.5.6.1 (see Testcase 3.5.1.1);
--source include/show_msg.inc

#Section 3.5.6.2
# Test case: Ensure that a trigger definition can specify a trigger action time of AFTER.
# See section 3.5.1.1
let $message= Testcase 3.5.6.2 (see Testcase 3.5.1.1);
--source include/show_msg.inc

#Section 3.5.6.3
# Test case: Ensure that a trigger definition that specifies a trigger action
#            time that is not either BEFORE or AFTER fails, with an appropriate
#            error message, at CREATE TRIGGER time.
let $message= Testcase 3.5.6.3:;
--source include/show_msg.inc

	--error ER_PARSE_ERROR
	Create trigger trg3_1 DURING UPDATE on tb3 for each row set new.f132=25;
	--error ER_PARSE_ERROR
	Create trigger trg3_2 TIME INSERT on tb3 for each row set new.f132=15;

#Cleanup
# OBN - Although none of the above should have been created we should do a cleanup
#       since if they have been created, not dropping them will affect following
#       tests.
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger tb3.trg3_1;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger tb3.trg3_2;
	--enable_warnings

#Section 3.5.6.4
# Test case: Ensure that a trigger defined with a trigger action time of BEFORE
#            always executes its triggered action immediately before the trigger event.
# See section 3.5.1.1
let $message= Testcase 3.5.6.4 (see Testcase 3.5.1.1);
--source include/show_msg.inc

#Section 3.5.6.5
# Test case: Ensure that a trigger defined with a trigger action time of AFTER
#            always executes its triggered action immediately after the trigger event.
let $message= Testcase 3.5.6.5 (see Testcase 3.5.1.1);
--source include/show_msg.inc

#############################
####### Section 3.5.7 #######
# Check on Trigger Event    #
#############################

#Section 3.5.7.1
#Test case: Ensure that a trigger definition can specify a trigger event of INSERT.
let $message= Testcase 3.5.7.1 (see Testcase 3.5.1.1);
--source include/show_msg.inc
	
#Section 3.5.7.2
# Test case: Ensure that a trigger definition can specify a trigger event of UPDATE.
let $message= Testcase 3.5.7.2 (see Testcase 3.5.1.1);
--source include/show_msg.inc

#Section 3.5.7.3
# Test case: Ensure that a trigger definition can specify a trigger event of DELETE.
let $message= Testcase 3.5.7.3 (see Testcase 3.5.1.1);
--source include/show_msg.inc

#Section 3.5.7.4
# Test case: Ensure that a trigger definition that specifies a trigger event that
#            is not either INSERT, UPDATE or DELETE fails, with an appropriate error
#            message, at CREATE TRIGGER time.
let $message= Testcase 3.5.7.4:;
--source include/show_msg.inc

	--error ER_PARSE_ERROR
	Create trigger trg4_1 BEFORE SELECT on tb3 for each row set new.f132=5;
	--error ER_PARSE_ERROR
	Create trigger trg4_2 AFTER VALUE on tb3 for each row set new.f132=1;

#Cleanup
# OBN - Although none of the above should have been created we should do a cleanup
#       since if they have been created, not dropping them will affect following
#       tests.
	--disable_warnings
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger tb3.trg4_1;
	--error 0, ER_TRG_DOES_NOT_EXIST
	drop trigger tb3.trg4_2;
	--enable_warnings

#Section 3.5.7.5 / 3.5.7.6
# Test case: Ensure that it is possible to create multiple BEFORE INSERT
#	     triggers on the same table
#
let $message= Testcase 3.5.7.5 / 3.5.7.6:;
--source include/show_msg.inc

	Create trigger trg5_1 BEFORE INSERT
		on tb3 for each row set new.f122='Trigger1 3.5.7.5/6';

	Create trigger trg5_2 BEFORE INSERT
		on tb3 for each row set new.f122='Trigger2 3.5.7.5';

	Insert into tb3 (f121,f122) values ('Test 3.5.7.5/6','Insert 3.5.7.5');
	Select f121,f122 from tb3 where f121='Test 3.5.7.5/6';
	update tb3 set f122='Update 3.5.7.6' where f121= 'Test 3.5.7.5/6';
	Select f121,f122 from tb3 where f121='Test 3.5.7.5/6';

#Cleanup
	--disable_warnings
	drop trigger trg5_1;
	drop trigger trg5_2;
	delete from tb3 where f121='Test 3.5.7.5/6';
	--enable_warnings


#Section 3.5.7.7 / 3.5.7.8
# Test case: Ensure that it is possible to create multiple AFTER INSERT
#            triggers on the same table
let $message= Testcase 3.5.7.7 / 3.5.7.8:;
--source include/show_msg.inc
	
	set @test_var='Before trig 3.5.7.7';
	Create trigger trg6_1 AFTER INSERT
		on tb3 for each row set @test_var='Trigger1 3.5.7.7/8';

	Create trigger trg6_2 AFTER INSERT
		on tb3 for each row set @test_var='Trigger2 3.5.7.7';

	select @test_var;
	Insert into tb3 (f121,f122) values ('Test 3.5.7.7/8','Insert 3.5.7.7');
	Select f121,f122 from tb3 where f121='Test 3.5.7.7/8';
	select @test_var;
	update tb3 set f122='Update 3.5.7.8' where f121= 'Test 3.5.7.7/8';
	Select f121,f122 from tb3 where f121='Test 3.5.7.7/8';
	select @test_var;

#Cleanup
	--disable_warnings
	drop trigger trg6_1;
	drop trigger trg6_2;
	delete from tb3 where f121='Test 3.5.7.7/8';	
	--enable_warnings


#Section 3.5.7.9 / 3.5.7.10
# Test case: Ensure that it is possible to create multiple BEFORE UPDATE
#            triggers on the same table
let $message= Testcase 3.5.7.9/10:;
--source include/show_msg.inc

	Create trigger trg7_1 BEFORE UPDATE
		on tb3 for each row set new.f122='Trigger1 3.5.7.9/10';

	Create trigger trg7_2 BEFORE UPDATE
		on tb3 for each row set new.f122='Trigger2 3.5.7.9';

	Insert into tb3 (f121,f122) values ('Test 3.5.7.9/10','Insert 3.5.7.9');
	Select f121,f122 from tb3 where f121='Test 3.5.7.9/10';
	update tb3 set f122='update 3.5.7.10' where f121='Test 3.5.7.9/10';
	Select f121,f122 from tb3 where f121='Test 3.5.7.9/10';

#Cleanup
	--disable_warnings
	drop trigger trg7_1;
	drop trigger trg7_2;
	delete from tb3 where f121='Test 3.5.7.9/10';	

#Section 3.5.7.11 / 3.5.7.12
# Test case: Ensure that it is possible to create multiple AFTER UPDATE
#            triggers on the same table
let $message= Testcase 3.5.7.11/12:;
--source include/show_msg.inc

	set @test_var='Before trig 3.5.7.11';
	Create trigger trg8_1 AFTER UPDATE
		on tb3 for each row set @test_var='Trigger 3.5.7.11/12';

	Create trigger trg8_2 AFTER UPDATE
		on tb3 for each row set @test_var='Trigger2 3.5.7.11';

	select @test_var;
	Insert into tb3 (f121,f122) values ('Test 3.5.7.11/12','Insert 3.5.7.11/12');
	select @test_var;
	Select f121,f122 from tb3 where f121='Test 3.5.7.11/12';
	update tb3 set f122='update 3.5.7.12' where f121='Test 3.5.7.11/12';
	Select f121,f122 from tb3 where f121='Test 3.5.7.11/12';
	select @test_var;
	delete from tb3 where f121='Test 3.5.7.11/12';

#Cleanup
	--disable_warnings
	drop trigger trg8_1;
	drop trigger trg8_2;
	delete from tb3 where f121='Test 3.5.7.11/12';	

#Section 3.5.7.13 / 3.5.7.14
# Test case: Ensure that it is possible to create multiple BEFORE DELETE
#            triggers on the same table
let $message= Testcase 3.5.7.13/14:;
--source include/show_msg.inc

	set @test_var=1;
	Create trigger trg9_1 BEFORE DELETE
		on tb3 for each row set @test_var=@test_var+1;

	Create trigger trg9_2 BEFORE DELETE
		on tb3 for each row set @test_var=@test_var+10;

	select @test_var;
	Insert into tb3 (f121,f122) values ('Test 3.5.7.13/14','Insert 3.5.7.13');
	Select f121,f122 from tb3 where f121='Test 3.5.7.13/14';
	select @test_var;
	delete from tb3 where f121='Test 3.5.7.13/14';	
	Select f121,f122 from tb3 where f121='Test 3.5.7.13/14';
	select @test_var;
	delete from tb3 where f121='Test 3.5.7.13/14';	
	select @test_var;

#Cleanup
	--disable_warnings
	drop trigger trg9_1;
	drop trigger trg9_2;
	delete from tb3 where f121='Test 3.5.7.13/14';	

#Section 3.5.7.15 / 3.5.7.16
# Test case: Ensure that possible to create multiple AFTER DELETE triggers
#            on the same table
let $message= Testcase 3.5.7.15/16:;
--source include/show_msg.inc
	
	set @test_var=1;
	Create trigger trg_3_406010_1 AFTER DELETE
		on tb3 for each row set @test_var=@test_var+5;

	Create trigger trg_3_406010_2 AFTER DELETE
		on tb3 for each row set @test_var=@test_var+50;

	--error ER_TRG_ALREADY_EXISTS
	Create trigger trg_3_406010_1 AFTER INSERT
		on tb3 for each row set @test_var=@test_var+1;

	select @test_var;
	Insert into tb3 (f121,f122) values ('Test 3.5.7.15/16','Insert 3.5.7.15/16');
	Select f121,f122 from tb3 where f121='Test 3.5.7.15/16';
	select @test_var;
	delete from tb3 where f121='Test 3.5.7.15/16';	
	Select f121,f122 from tb3 where f121='Test 3.5.7.15/16';
	select @test_var;
	delete from tb3 where f121='Test 3.5.7.15/16';	
	select @test_var;

#Cleanup
	--disable_warnings
	drop trigger trg_3_406010_1;
	drop trigger trg_3_406010_2;
	delete from tb3 where f121='Test 3.5.7.15/16';	
	--enable_warnings


#Section 3.5.7.17
# Test case: Ensure that it is possible to have a BEFORE INSERT, an AFTER INSERT,
#            a BEFORE UPDATE, an AFTER UPDATE, a BEFORE DELETE, and an AFTER DELETE
#            trigger on the same table; that is, ensure that every persistent base
#            table may be the subject table for exactly six triggers
let $message= Testcase 3.5.7.17 (see Testcase 3.5.1.1);
--source include/show_msg.inc


# Cleanup section 3.5
	connection default;
        disconnect con1_general;
        disconnect con1_super;
	drop user test_general@localhost;
	drop user test_general;
	drop user test_super@localhost;

DROP TABLE test.tb3;


set GLOBAL sql_mode=default;
