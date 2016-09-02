#! /bin/bash
#
# collect.sh
# Copyright (C) 2016 Martine Lenders <mail@martine-lenders.eu>
#
# Distributed under terms of the MIT license.
#

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ $# -lt 2 ]; then
    echo "Usage: $0 iotlab-user iotlab-site"
    exit 1
fi

ssh $1@$2.iot-lab.info 'serial_aggregator' | \
        sed -e 's/.*> //' -e 's/ //g' | \
        xxd -r -ps > ${DIR}/at86rf2xx_nrg
