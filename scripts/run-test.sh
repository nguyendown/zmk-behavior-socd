#!/bin/sh

# Copyright (c) 2020 The ZMK Contributors
# SPDX-License-Identifier: MIT

zephyr_base=$(west topdir)

if [ -z "$zephyr_base" ]; then
    exit 1
fi

module_root=$(readlink -f "${MODULE_ROOT}" 2>/dev/null)

if [ -z "$module_root" ]; then
    script_path=$(readlink -f "$0" 2>/dev/null)
    script_dir=$(dirname "$script_path")
    module_root=$(dirname "$script_dir")
fi

if [ -z "$module_root" ]; then
    echo "Cannot detect module root. Please specify it via MODULE_ROOT."
    exit 1
fi

path=$(readlink -f "$1" 2>/dev/null)

if [ -z "$1" ]; then
    path="$module_root/tests"
fi

build_dir="$module_root/build"

testcases=$(find "$path" -name native_posix_64.keymap -exec dirname \{\} \;)
num_cases=$(echo "$testcases" | wc -l)
if [ $num_cases -gt 1 ] || [ "$testcases" != "$path" ]; then
    echo "" > "$build_dir/tests/pass-fail.log"
    echo "$testcases" | xargs -L 1 -P ${J:-4} "$module_root/scripts/run-test.sh"
    err=$?
    sort -k2 "$build_dir/tests/pass-fail.log"
    exit $err
fi

testcase=${path#"$module_root"/}
app_dir=$zephyr_base/app
echo "Running $testcase:"

# echo west build -d "$build_dir/$testcase" -b native_posix_64 "$app_dir" -- -DCONFIG_ASSERT=y -DZMK_CONFIG="$path" -DZMK_EXTRA_MODULES="$module_root"
west build -d "$build_dir/$testcase" -b native_posix_64 "$app_dir" -- -DCONFIG_ASSERT=y -DZMK_CONFIG="$path" -DZMK_EXTRA_MODULES="$module_root" > /dev/null 2>&1
if [ $? -gt 0 ]; then
    echo "FAILED: $testcase did not build" | tee -a "$build_dir/tests/pass-fail.log"
    exit 1
fi

"$build_dir/$testcase/zephyr/zmk.exe" | sed -e "s/.*> //" | tee "$build_dir/$testcase/keycode_events_full.log" | sed -n -f "$path/events.patterns" > "$build_dir/$testcase/keycode_events.log"
diff -auZ "$path/keycode_events.snapshot" "$build_dir/$testcase/keycode_events.log"
if [ $? -gt 0 ]; then
    if [ -f "$testcase/pending" ]; then
        echo "PENDING: $testcase" | tee -a "$build_dir/tests/pass-fail.log"
        exit 0
    fi


    if [ -n "${ZMK_TESTS_AUTO_ACCEPT}" ]; then
        echo "Auto-accepting failure for $testcase"
        cp "$build_dir/$testcase/keycode_events.log" "$path/keycode_events.snapshot"
    else
        echo "FAILED: $testcase" | tee -a "$build_dir/tests/pass-fail.log"
        exit 1
    fi
fi

echo "PASS: $testcase" | tee -a "$build_dir/tests/pass-fail.log"
exit 0
