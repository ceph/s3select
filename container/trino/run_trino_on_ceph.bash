#!/bin/bash

root_dir()
{
  cd $(git rev-parse --show-toplevel)
}

modify_end_point_on_hive_properties()
{
#not in use
return;
#TODO if ./trino/catalog/hive.properties exist

  [ $# -lt 1 ] && echo type s3-endpoint-url && return
  root_dir
  export S3_ENDPOINT=$1
  cat container/trino/trino/catalog/hive.properties  | awk -v x=${S3_ENDPOINT:-NO_SET} '{if(/hive.s3.endpoint/){print "hive.s3.endpoint="x"\n";} else {print $0;}}' > /tmp/hive.properties
  cp /tmp/hive.properties container/trino/trino/catalog/hive.properties
  cat ./container/trino/hms_trino.yaml | awk -v x=${S3_ENDPOINT:-NOT_SET} '{if(/[ *]- S3_ENDPOINT/){print "\t- S3_ENDPOINT="x"\n";} else {print $0;}}' > /tmp/hms_trino.yaml
  cp /tmp/hms_trino.yaml ./container/trino/hms_trino.yaml
  cd -
}

trino_exec_command()
{
## run SQL statement on trino 
  sudo docker exec -it trino /bin/bash -c "time trino --catalog hive --schema cephs3 --execute \"$@\""
}

boot_trino_hms()
{
  root_dir
  [ -z ${S3_ENDPOINT} ] && echo "missing end-variable S3_ENDPOINT (URL)" && return
  [ -z ${S3_ACCESS_KEY} ] && echo missing end-variable S3_ACCESS_KEY && return
  [ -z ${S3_SECRET_KEY} ] && echo missing end-variable S3_SECRET_KEY && return

  # modify hms_trino.yaml according to user setup (environment variables)
  cat ./container/trino/hms_trino.yaml | \
  awk -v x=${S3_ENDPOINT:-NOT_SET} '{if(/- S3_ENDPOINT/){print "      - S3_ENDPOINT="x;} else {print $0;}}' | \
  awk -v x=${S3_ACCESS_KEY:-NOT_SET} '{if(/- S3_ACCESS_KEY/){print "      - S3_ACCESS_KEY="x;} else {print $0;}}' | \
  awk -v x=${S3_SECRET_KEY:-NOT_SET} '{if(/- S3_SECRET_KEY/){print "      - S3_SECRET_KEY="x;} else {print $0;}}' > /tmp/hms_trino.yaml
  cp /tmp/hms_trino.yaml ./container/trino/hms_trino.yaml



  # modify hive.properties according to user setup (environment variables)
  cat container/trino/trino/catalog/hive.properties | \
  awk -v x=${S3_ENDPOINT:-NO_SET} '{if(/hive.s3.endpoint/){print "hive.s3.endpoint="x"\n";} else {print $0;}}' | \
  awk -v x=${S3_ACCESS_KEY:-NO_SET} '{if(/hive.s3.aws-access-key/){print "hive.s3.aws-access-key="x;} else {print $0;}}' | \
  awk -v x=${S3_SECRET_KEY:-NO_SET} '{if(/hive.s3.aws-secret-key/){print "hive.s3.aws-secret-key="x;} else {print $0;}}' > /tmp/hive.properties
  cp /tmp/hive.properties ./container/trino/trino/catalog/hive.properties

  sudo docker compose -f ./container/trino/hms_trino.yaml up -d
  cd -
}

shutdown_trino_hms()
{
  root_dir
  sudo docker compose -f ./container/trino/hms_trino.yaml down
  cd -
}

trino_create_table()
{
table_name=$1
create_table_comm="create table hive.cephs3.${table_name}(c1 varchar,c2 varchar,c3 varchar,c4 varchar, c5 varchar,c6 varchar,c7 varchar,c8 varchar,c9 varchar,c10 varchar)
 WITH ( external_location = 's3a://hive/warehouse/cephs3/${table_name}/',format = 'TEXTFILE',textfile_field_separator = ',');"
sudo docker exec -it trino /bin/bash -c "trino --catalog hive --schema cephs3 --execute \"${create_table_comm}\""
}

tpcds_cli()
{
## a CLI example for generating TPCDS data
sudo docker run --env S3_ENDPOINT=172.17.0.1:8000 --env S3_ACCESS_KEY=b2345678901234567890 --env S3_SECRET_KEY=b234567890123456789012345678901234567890 --env BUCKET_NAME=hive --env SCALE=2 -it galsl/hadoop:tpcds bash -c '/root/run_tpcds_with_scale'
}

update_table_external_location()
{
root_dir
[ -z ${BUCKET_NAME} ] && echo need to define BUCKET_NAME && return
[ -z ${SCALE} ] && echo need to define SCALE && return

cat TPCDS/ddl/create_tpcds_tables.sql  | sed "s/tpcds2\/4/${BUCKET_NAME}\/SCALE_${SCALE}/"
}
 
