#!/bin/bash

run_tpcds()
{
## END_POINT_IP=172.21.48.86 // RGW end point ip (local or remote)
## SCALE (2-1000) the bigger the SCALE, the longer it takes, and also thee more space is taken.
## the `sleep 20` is for the HADOOP. it needs some wait time, otherwise it may get into "safe mode" and will abort execution

## the following command executed within a dedicated container, it will connect the HADOOP to a running RGW, it will boot HADOOP, and will run the TPCDS data-set generator.
## the results reside on CEPH object storage.
sudo docker run --name  tpcds_generate --rm --env SCALE=2 --env END_POINT_IP=172.21.48.86 -it galsl/hadoop:presto_hive_conn  sh -c \
'/work/generate_key.bash;
. /etc/bashrc;
deploy_ceph_s3a_ip $END_POINT_IP;
start_hadoop;
sleep 20;
start_tpcds;'

}

move_from_tpcds_bucket_to_hive_bucket()
{
## for the case it needs to move into different bucket(where trino is point at)
## its is also possible to chage the `create table ... external_location = ...` statements

aws s3 sync s3://tpcds2 s3://hive
}

trino_load_all_tpcds_tables_into_external()
{
## running create_tpcds_tables.sql, the "create_tpcds_tables.sql" should reside in trino container
sudo docker exec -it trino /bin/bash -c 'time trino --catalog hive --schema cephs3 -f create_tpcds_tables.sql'
}

trino_show_tables()
{
## running any SQL statement in Trino client.
sudo docker exec -it trino /bin/bash -c 'trino --catalog hive --schema cephs3 --execute "show tables;";'
}

