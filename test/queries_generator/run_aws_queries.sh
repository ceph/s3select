#!/bin/sh
set -x
set -e

./generate_aws_cmds

chmod +x aws_cmds.sh

./aws_cmds.sh
