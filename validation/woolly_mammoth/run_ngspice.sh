#!/usr/bin/env sh
set -eu

validation_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
output_dir=${1:-"$validation_dir/output"}
mkdir -p "$output_dir"

if ! command -v ngspice >/dev/null 2>&1; then
    echo "ngspice is required (for example: brew install ngspice)." >&2
    exit 2
fi

cd "$output_dir"
ngspice -b -o woolly_mammoth_ngspice.log \
    "$validation_dir/woolly_mammoth_ngspice.cir"

awk '
BEGIN {
    print "time_s,output_v,node_B1_v,node_C1_NODE_v,node_E2_v,node_C2_NODE_v"
    have_time = 0
}
NR > 1 && NF >= 6 && (!have_time || $1 > last_time) {
    printf "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n", $1, $2, $3, $4, $5, $6
    last_time = $1
    have_time = 1
}
' woolly_mammoth_ngspice.dat > woolly_mammoth_ngspice.csv

test "$(wc -l < woolly_mammoth_ngspice.csv)" -gt 100
echo "Created $output_dir/woolly_mammoth_ngspice.csv"
