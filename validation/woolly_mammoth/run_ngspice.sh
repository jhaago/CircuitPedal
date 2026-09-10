#!/usr/bin/env sh
set -eu

validation_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
output_dir=${1:-"$validation_dir/output"}
mkdir -p "$output_dir"

supply=${SUPPLY:-9}
input_fs=${INPUT_FS:-0.25}
frequency=${FREQ:-110}
wool=${WOOL:-0.75}
pinch=${PINCH:-0.35}
eq_position=${EQPOS:-0.50}
output_position=${OUTPUT_POS:-0.70}

if ! command -v ngspice >/dev/null 2>&1; then
    echo "ngspice is required (for example: brew install ngspice)." >&2
    exit 2
fi

sed \
    -e "s/^\.param SUPPLY=.*/.param SUPPLY=$supply/" \
    -e "s/^\.param INPUT_FS=.*/.param INPUT_FS=$input_fs/" \
    -e "s/^\.param FREQ=.*/.param FREQ=$frequency/" \
    -e "s/^\.param WOOL=.*/.param WOOL=$wool/" \
    -e "s/^\.param PINCH=.*/.param PINCH=$pinch/" \
    -e "s/^\.param EQPOS=.*/.param EQPOS=$eq_position/" \
    -e "s/^\.param OUTPUT_POS=.*/.param OUTPUT_POS=$output_position/" \
    "$validation_dir/woolly_mammoth_ngspice.cir" \
    > "$output_dir/woolly_mammoth_effective.cir"

cd "$output_dir"
ngspice -b -o woolly_mammoth_ngspice.log \
    woolly_mammoth_effective.cir

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
