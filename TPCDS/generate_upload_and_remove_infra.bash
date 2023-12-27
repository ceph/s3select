#!/bin/bash

## this script resides in [galsl/fedora_38:tpcds_v2] docker container.
## the container uses the following repo [ https://github.com/galsalomon66/tpc-ds-datagen-to-aws-s3 ] for the dsdgen application.
## the purpose of this script it to launch multiple instances of the dsdgen-application(depends on number of cores)
## the flow splits between the very-big-tables and the small tables.
## the num_of_cpu defines the size of parallelism, the num_of_partitions defines the amount chunks that combines togather a single table (it could be huge).
## each cycle of parallel generate-application, ended with flow that uploads the generated files into S3(its done in parallel), upon all files are uploaded
## it removes all files, i.e. for 3TB scale there is no need for 3TB of disk-space (as for S3-storage capacity it obvious ...)


## TODO set by te user
TPCDS_DIR=/tpcds_output/


all_tables="call_center
catalog_page
customer_address
customer
customer_demographics
date_dim
household_demographics
income_band
item
promotion
reason
ship_mode
store
time_dim
warehouse
web_page
web_site
catalog_returns
catalog_sales
web_returns
web_sales
store_returns
store_sales"

#big tables and also parent
#parent table means it got a child table, i.e. there is a relation between them.
parent_tables="store_sales catalog_sales web_sales inventory"

## not a parent table
standalone_tables="call_center catalog_page customer_address customer customer_demographics date_dim household_demographics income_band
item promotion reason ship_mode store time_dim warehouse web_page web_site"
#small_tables=""

num_of_cpu=56
num_of_partitions=0

create_dsdgen_workers_non_parent_tables()
{

[ ! -d ${TPCDS_DIR} ] && echo ${TPCDS_DIR} not exist && exit
num_of_partitions=$(echo 1 | awk -v sc=${SCALE} -v c=${num_of_cpu} '{print int((sc/1000)*c);}')
if [  $num_of_partitions -le 1 ]
then 
	num_of_partitions=2
fi

echo "small tables="num_of_partitions=${num_of_partitions}

((i=1))

for t in ${standalone_tables}
do
        for c in $(seq 1 ${num_of_partitions})
        do
		## the command line defines which table, what scale(size), paratition size, what partition to produce and where to produce it.
        	echo "time ./dsdgen -dir ${TPCDS_DIR} -table ${t} -scale ${SCALE} -force -parallel ${num_of_partitions} -child ${c} &" >> generate_upload_and_remove_exec.bash
		## number of CPU
        	if [ $(( i++ % ${num_of_cpu} )) -eq 0 ]
       		 then
                	echo wait >> generate_upload_and_remove_exec.bash
			# upon complete with wait, loop on generated dat files, upload each in parallel, each upload is done, remove file
			# upload && remove
			# 
			echo upload_and_remove_worker_func >> generate_upload_and_remove_exec.bash
        	fi
        done
done
echo wait >> generate_upload_and_remove_exec.bash
echo upload_and_remove_worker_func >> generate_upload_and_remove_exec.bash
echo "echo small tables done." >> generate_upload_and_remove_exec.bash

chmod +x generate_upload_and_remove_exec.bash
}

create_dsdgen_workers()
{

[ ! -d ${TPCDS_DIR} ] && echo ${TPCDS_DIR} not exist && exit
num_of_partitions=$(echo 1 | awk -v sc=${SCALE} -v c=${num_of_cpu} '{print int((sc/10)*c);}')
echo "big tables="num_of_partitions=${num_of_partitions}
if [  $num_of_partitions -le 1 ]
then 
	num_of_partitions=2
fi

((i=1))
touch generate_upload_and_remove_exec.bash
rm -f generate_upload_and_remove_exec.bash

echo "#!/bin/bash" >> generate_upload_and_remove_exec.bash
## upload_and_remove_func.bash include functions for upload and remove
echo ". generate_upload_and_remove_infra.bash" >> generate_upload_and_remove_exec.bash
echo "cd /tpc-ds-datagen-to-aws-s3/tpc-ds/v2.11.0rc2/tools" >> generate_upload_and_remove_exec.bash

for t in ${parent_tables}
do
        for c in $(seq 1 ${num_of_partitions})
        do
        	echo "time ./dsdgen -dir ${TPCDS_DIR} -table ${t} -scale ${SCALE} -force -parallel ${num_of_partitions} -child ${c} &" >> generate_upload_and_remove_exec.bash
		## number of CPU
        	if [ $(( i++ % ${num_of_cpu} )) -eq 0 ]
       		 then
                	echo wait >> generate_upload_and_remove_exec.bash
			# upon complete with wait, loop on generated dat files, upload each in parallel, each upload is done, remove file
			# upload && remove
			# 
			echo upload_and_remove_worker_func >> generate_upload_and_remove_exec.bash
        	fi
        done
done
echo wait >> generate_upload_and_remove_exec.bash
echo upload_and_remove_worker_func >> generate_upload_and_remove_exec.bash
echo "echo big tables done." >> generate_upload_and_remove_exec.bash

## adding the production of the other tables
create_dsdgen_workers_non_parent_tables

chmod +x generate_upload_and_remove_exec.bash

## the generated script bellow contains all is needed for creating TPCDS tables in S3-storage.
## should execute by the user
#./generate_upload_and_remove_exec.bash

}

upload_and_remove_worker_func()
{
# create list of tasks to run in background, remove each uploaded file upon completion 
(i=0)
touch upload_and_remove_exec.bash
rm -f upload_and_remove_exec.bash

echo "#!/bin/bash" >> upload_and_remove_exec.bash

for f in $(ls ${TPCDS_DIR}/*.dat)
do
	#echo $f
	table_name=$(basename $f | sed 's/_[0-9]\+_[0-9]\+/ /' | awk '{print $1;}')
	echo "(aws s3api put-object --bucket hive --key scale_${SCALE}/${table_name}/$(basename $f) --body ${f} --endpoint-url ${S3_ENDPOINT} > /dev/null 2>&1 && echo upload ${f} && rm -f ${f}) &" >> upload_and_remove_exec.bash 
	if [ $(( i++ % ${num_of_cpu} )) -eq 0 ]
       	then
		echo wait >> upload_and_remove_exec.bash
	fi
done

echo wait >> upload_and_remove_exec.bash
#upload and remove all generated files 
chmod +x upload_and_remove_exec.bash
cp upload_and_remove_exec.bash upload_and_remove.bash_${RANDOM} ## debug

## start upload and remove in parallel
./upload_and_remove_exec.bash

}

