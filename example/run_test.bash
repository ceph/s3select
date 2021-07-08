#!/bin/bash

set -e

PREFIX=${1:-"./example"} 

## purpose : sanity tests

s3select_calc() 
{
l="$*"  
res=$( echo 1 | "$PREFIX"/s3select_example -q  "select ${l} from stdin;" ) 
echo "$res" | sed 's/.$//'
}

# create c file with expression , compile it and run it.
c_calc()
{
cat << @@ > "$PREFIX"/tmp.c

#include <stdio.h>
int main()
{
printf("%f\n",$*);
}
@@
gcc -o "$PREFIX"/a.out "$PREFIX"/tmp.c
"$PREFIX"/a.out
}

expr_test()
{
## test the arithmetic evaluation of s3select against C program 
for i in {1..100}
do
	e=$(python3 "$PREFIX"/expr_genrator.py 5)
	echo expression["$i"]="$e"
	r1=$(s3select_calc "$e")
	r2=$(c_calc "$e")
    echo "$r1" "$r2"

	## should be zero or very close to zero; ( s3select is C compile program )
    res=$(echo "" | awk -v e="$e" -v r1="$r1" -v r2="$r2" 'function abs(n){if (n<0) return -n; else return n;}{if (abs(r1-r2) > 0.00001) {print "MISSMATCH result for expression",e;}}')
    if test "$res" != ""; then
        echo "$res"
        exit 1
    fi
done
}

aggregate_test()
{
## generate_rand_csv is generating with the same seed 
echo check sum 
s3select_val=$("$PREFIX"/generate_rand_csv 10 10 | "$PREFIX"/s3select_example -q 'select sum(int(_1)) from stdin;') 
awk_val=$("$PREFIX"/generate_rand_csv 10 10 | awk 'BEGIN{FS=",";} {s+=$1;} END{print s;}')
s3select_val=${s3select_val::-1}
echo "$s3select_val" "$awk_val"
if test "$s3select_val" -ne "$awk_val"; then
    exit 1
fi
echo check min 
s3select_val=$("$PREFIX"/generate_rand_csv 10 10 | "$PREFIX"/s3select_example -q 'select min(int(_1)) from stdin;') 
awk_val=$("$PREFIX"/generate_rand_csv 10 10 | awk 'BEGIN{FS=",";min=100000;} {if(min>$1) min=$1;} END{print min;}')
s3select_val=${s3select_val::-1}
echo "$s3select_val" "$awk_val"
if test "$s3select_val" -ne "$awk_val"; then
    exit 1
fi
echo check max 
s3select_val=$("$PREFIX"/generate_rand_csv 10 10 | "$PREFIX"/s3select_example -q 'select max(int(_1)) from stdin;') 
awk_val=$("$PREFIX"/generate_rand_csv 10 10 | awk 'BEGIN{FS=",";max=0;} {if(max<$1) max=$1;} END{print max;}' )
s3select_val=${s3select_val::-1}
echo "$s3select_val" "$awk_val"
if test "$s3select_val" -ne "$awk_val"; then
    exit 1
fi
echo check substr and count 
s3select_val=$("$PREFIX"/generate_rand_csv 10000 10 | "$PREFIX"/s3select_example -q 'select count(int(_1)) from stdin where int(_1)>200 and int(_1)<250;')
awk_val=$("$PREFIX"/generate_rand_csv 10000 10 | "$PREFIX"/s3select_example -q 'select substring(_1,1,1) from stdin where int(_1)>200 and int(_1)<250;' | uniq -c | awk '{print $1;}')
s3select_val=${s3select_val::-1}
echo "$s3select_val" "$awk_val"
if test "$s3select_val" -ne "$awk_val"; then
    exit 1
fi
}

parquet_test()
{
s3select_val=$(${PREFIX}/s3select_example -q "select count(*) from $(realpath parquet_mix_types.parquet) where _1>555 and _1<777;" | grep -v '^\[') 

if test "${s3select_val}" != "221,"; then
  echo "parquet test failed,${s3select_val}"
#  exit
fi

s3select_val=$(${PREFIX}/s3select_example -q "select c5,c1,int(_1*0+6),int(_3*0+4),substring(c1,int(_1*0+6),int(_3*0+4)) from $(realpath parquet_mix_types.parquet) where ((c1 like \"%wedd%\") and c0 <100 ) and c5 between 2.1000000000000001 and 2.6200000000000001 and c4 between \"col4_1\" and \"col4_2\";" | grep -v '^\[')

echo ${s3select_val}
}

###############################################################

expr_test
aggregate_test
parquet_test

rm "$PREFIX"/tmp.c "$PREFIX"/a.out

exit 0

