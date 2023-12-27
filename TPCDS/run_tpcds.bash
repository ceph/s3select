#!/bin/bash

## this script is the entry-point of tpcds-data-generation
## the first and the only argument is the SCALE factor

[ $# -lt 1 ] && echo "type a single number for the scale (2 --> 3000)" && exit 

re='^[0-9]+$'
[[ ! $1 =~ $re ]] && echo "SCALE should be a number" && exit

## the following code lines accepts env-variables for the S3 system
[ -z ${S3_ENDPOINT} ] && echo "missing env-variable S3_ENDPOINT" && exit
[ -z ${S3_ACCESS_KEY} ] && echo missing env-variable S3_ACCESS_KEY && exit
[ -z ${S3_SECRET_KEY} ] && echo missing env-variable S3_SECRET_KEY && exit

## updating AWS credentials
cat ~/.aws/credentials | \
        awk -v acc=${S3_ACCESS_KEY} '{if($0 ~ /aws_access_key_id/){print "aws_access_key_id = ",acc;} else{print $0;}}' | \
        awk -v acc=${S3_SECRET_KEY} '{if($0 ~ /aws_secret_access_key/){print "aws_secret_access_key = ",acc;} else{print $0;}}' > /tmp/credentials

cat /tmp/credentials > ~/.aws/credentials

export SCALE=$1

. ./generate_upload_and_remove_infra.bash

## create generate_upload_and_remove_exec.bash
create_dsdgen_workers
## running tpcds data generator script
time /generate_upload_and_remove_exec.bash

