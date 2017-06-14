#!/usr/bin/perl
use strict;
no warnings;
use Test::More;
use DBI;
use Getopt::Long;

my $dbname;
my $username;
my $password;
my $host;
GetOptions ( "--host=s" => \$host,
    "--dbname=s" => \$dbname,
    "--username=s" => \$username,
    "--password=s" => \$password);
my $dbh = DBI->connect("dbi:Pg:dbname=$dbname; host=$host", "$username", "$password",
    {PrintError => 0});
ok($dbh->err == 0) or (print $DBI::errstr and BAIL_OUT);

my $query = "DELETE FROM test_results;";
$dbh->do($query);
ok($dbh->err == 0) or (print $DBI::errstr and $dbh->disconnect() and BAIL_OUT);

$query = "SELECT schedule.create_job(NULL, '');";
my $sth = $dbh->prepare($query);
$sth->execute();
ok($dbh->err == 0) or (print $DBI::errstr and $dbh->disconnect() and BAIL_OUT);
my $job_id = $sth->fetchrow_array() and $sth->finish();
$sth->finish();

$query = "SELECT schedule.set_job_attributes(?, \'{ \"name\": \"Test\",
    \"cron\": \"*/3 * * * *\",
    \"commands\": [\"INSERT INTO test_results (time_mark, commentary) VALUES(now(), ''jobNextTime'')\",
                    \"INSERT INTO test_results (time_mark, commentary) VALUES(now(), ''jobNextTime'')\"],
    \"run_as\": \"tester\",
    \"use_same_transaction\": \"true\",
    \"next_time_statement\": \"SELECT now() + interval ''1 minute'';\"
    }\')";
$sth = $dbh->prepare($query);
$sth->bind_param(1, $job_id);
ok($sth->execute()) or (print $DBI::errstr and $dbh->disconnect() and BAIL_OUT);
$sth->finish();

sleep 190;
my $query = "DELETE FROM test_results;";
$dbh->do($query);
ok($dbh->err == 0) or (print $DBI::errstr and $dbh->disconnect() and BAIL_OUT);

sleep 70;
$query = "SELECT count(*) FROM test_results";
$sth = $dbh->prepare($query);
ok($sth->execute()) or (print $DBI::errstr and $dbh->disconnect() and BAIL_OUT);

my $result = $sth->fetchrow_array() and $sth->finish();
ok ($result > 1) or print "Count <= 1\n";

$query = "SELECT schedule.deactivate_job(?)";
$sth = $dbh->prepare($query);
$sth->bind_param(1, $job_id);
ok($sth->execute()) or print $DBI::errstr ;
$sth->finish();

$query = "DELETE FROM test_results;";
$dbh->do($query);
ok($dbh->err == 0) or print $DBI::errstr;

$query = "SELECT schedule.drop_job(?)";
$sth = $dbh->prepare($query);
$sth->bind_param(1, $job_id);
ok($sth->execute()) or print $DBI::errstr;
$sth->finish();

$dbh->disconnect();

done_testing();

