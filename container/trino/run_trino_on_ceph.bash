#!/bin/bash

root_dir()
{
  cd $(git rev-parse --show-toplevel)
}

modify_end_point_on_hive_properties()
{
#TODO if ./trino/catalog/hive.properties exist

  [ $# -lt 1 ] && echo type s3-endpoint-url && return
  root_dir
  export S3_ENDPOINT=$1
  cat container/trino/trino/catalog/hive.properties  | awk -v x=${S3_ENDPOINT} '{if(/hive.s3.endpoint/){print "hive.s3.endpoint="x"\n";} else {print $0;}}' > /tmp/hive.properties
  cp /tmp/hive.properties container/trino/trino/catalog/hive.properties
  cd -
}

trino_exec_command()
{
## run SQL statement on trino 
  root_dir
  echo $@ > ./container/trino/work/query.trino;
  sudo docker exec -it trino /bin/bash -c 'time trino --catalog hive --schema cephs3 -f work/query.trino'
  cd -
}

boot_trino_hms()
{
  root_dir
  sudo docker compose -f ./container/trino/hms_trino.yaml up -d  
  cd -
}

shutdown_trino_hms()
{
  root_dir
  sudo docker compose -f ./container/trino/hms_trino.yaml down
  cd -
}

