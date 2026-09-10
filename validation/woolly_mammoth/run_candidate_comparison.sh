#!/usr/bin/env sh
set -eu

validation_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(CDPATH= cd -- "$validation_dir/../.." && pwd)
validator=${1:-"$repository_dir/build/circuitpedal_validate"}
output_root=${2:-"$validation_dir/output/candidate_comparison"}

if [ ! -x "$validator" ]; then
    echo "CircuitPedal validator is not executable: $validator" >&2
    exit 2
fi

run_case()
{
    case_name=$1
    wool=$2
    pinch=$3
    eq_position=$4
    output_position=$5
    case_dir="$output_root/$case_name"
    mkdir -p "$case_dir"

    SUPPLY=9 INPUT_FS=0.25 FREQ=110 \
        WOOL="$wool" PINCH="$pinch" EQPOS="$eq_position" \
        OUTPUT_POS="$output_position" \
        "$validation_dir/run_ngspice.sh" "$case_dir"

    "$validator" render \
        "$repository_dir/circuits/woolly_mammoth_reference_draft.cpedal" \
        "$case_dir/circuitpedal.csv" \
        --sample-rate 192000 --seconds 1 \
        --signal sine --frequency 110 --amplitude 0.25 \
        --control "WOOL=$wool" --control "PINCH=$pinch" \
        --control "EQ=$eq_position" --control "OUTPUT=$output_position" \
        --node B1 --node C1_NODE --node E2 --node C2_NODE

    report="$case_dir/comparison.txt"
    : > "$report"
    for column in output_v node_B1_v node_C1_NODE_v node_E2_v node_C2_NODE_v
    do
        {
            echo "case=$case_name column=$column"
            "$validator" compare \
                "$case_dir/woolly_mammoth_ngspice.csv" \
                "$case_dir/circuitpedal.csv" \
                --column "$column" \
                --start 0.25 --duration 0.50 --max-lag 64 \
                --fundamental 110 --harmonics 8
        } | tee -a "$report"
    done
}

# Nominal defaults, an all-controls-maximum state that mirrors the physical DC
# campaign, and a lower-Wool/high-Pinch state that exercises a less bypassed Q2.
run_case nominal 0.75 0.35 0.50 0.70
run_case all_maximum 1.00 1.00 1.00 1.00
run_case restrained 0.25 0.70 0.25 0.70

echo "Created Woolly candidate comparison under $output_root"
