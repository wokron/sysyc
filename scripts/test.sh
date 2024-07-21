#!/bin/bash

# this script is used for integration testing

# Options:
# -s: Stage to test, includes: ir, ir-opt, asm, asm-opt
# -t: Test to run, includes: all, func, perf, hidden_func, final_perf
# -f: Test only the specified file, like `func/test.sy`

TESTFILES_DIR="testfiles"
CC="gcc"

# Default values
stage="ir"
tests=""
file=""
while getopts "s:t:f:" opt; do
    case ${opt} in
    s)
        stage=$OPTARG
        ;;
    t)
        # if arg is not `all`, test can be multiple values
        if [ $OPTARG != "all" ]; then
            # append optarg to tests
            tests="$tests $OPTARG"
        fi
        ;;
    f)
        file=$OPTARG
        ;;
    \?)
        echo "invalid option: $OPTARG" 1>&2
        exit 1
        ;;
    :)
        echo "invalid option: $OPTARG requires an argument" 1>&2
        exit 1
        ;;
    esac
done

testfiles=""
# if file is not empty, test only the specified file
if [ ! -z "$file" ]; then
    testfiles=$file
else
    # if file is empty, find test files for each test category
    if [ "$tests" == "all" ]; then
        testfiles=$(find $TESTFILES_DIR -name "*.sy")
    else
        for test in $tests; do
            testfiles="$testfiles $(find $TESTFILES_DIR/$test -name "*.sy")"
        done
    fi
fi

# show test information
echo "=========== test configuration ==========="
echo "stage: $stage"
echo "tests: $tests"
echo "testfiles:"
for testfile in $testfiles; do
    echo "  $testfile"
done
echo "total count: $(echo $testfiles | wc -w)"
echo

# build the project by cmake
bash ./scripts/build.sh >/dev/null || ! echo "failed to build the project" || exit 1

SYSYC="./build/sysyc"

echo "============== running tests ============="

test_no="1"
total_tests=$(echo $testfiles | wc -w)
pass_count="0"
fail_count="0"

# run tests
for testfile in $testfiles; do
    # get the filename without the extension
    testfile_without_ext=${testfile%.*}
    filename=$(basename -- "$testfile_without_ext")

    input_file=${testfile_without_ext}.in
    expect_file=${testfile_without_ext}.out

    if [ $stage == "ir" ] || [ $stage == "ir-opt" ]; then
        ir_file=${filename}.ssa
        opt=""
        if [ $stage == "ir-opt" ]; then
            opt="-O1"
        fi
        $SYSYC --emit-ir -o $ir_file $testfile $opt || ! echo "$testfile failed to generate ir file x" || continue

        output_file=${filename}.out

        # if exist input file, run the ir file with input
        if [ -f $input_file ]; then
            bash ./scripts/qbe_run.sh $ir_file <$input_file >$output_file 2>/dev/null
        else
            bash ./scripts/qbe_run.sh $ir_file >$output_file 2>/dev/null
        fi

        # append return value to the output file
        ret_val=$?
        sed -i '$a\' $output_file
        echo $ret_val >>$output_file

        # compare the content of output_file with expected_output_file
        if [ -f $expect_file ]; then
            diff -Z $expect_file $output_file
            if [ $? -eq 0 ]; then
                echo "($test_no/$total_tests) $testfile passed ✓"
                pass_count=$((pass_count + 1))
            else
                echo "($test_no/$total_tests) $testfile failed x"
                fail_count=$((fail_count + 1))
            fi
        fi

        rm $ir_file $output_file
    else
        asm_file=${filename}.s
        opt=""
        if [ $stage == "asm-opt" ]; then
            opt="-O1"
        fi
        $SYSYC -S -o $asm_file $testfile $opt || ! echo "$testfile failed to generate asm file x" || continue

        output_file=${filename}.out

        riscv_run="./scripts/riscv_run.sh"
        # if docker exists, use docker to run the asm file
        if [ -x "$(command -v docker)" ]; then
            riscv_run="./scripts/riscv_docker_run.sh"
        fi

        # if exist input file, run the asm file with input
        if [ -f $input_file ]; then
            bash $riscv_run $asm_file <$input_file >$output_file 
        else
            bash $riscv_run $asm_file >$output_file 
        fi

        # append return value to the output file
        ret_val=$?
        sed -i '$a\' $output_file
        echo $ret_val >>$output_file

        # compare the content of output_file with expected_output_file
        if [ -f $expect_file ]; then
            diff -Z $expect_file $output_file
            if [ $? -eq 0 ]; then
                echo "($test_no/$total_tests) $testfile passed ✓"
                pass_count=$((pass_count + 1))
            else
                echo "($test_no/$total_tests) $testfile failed x"
                fail_count=$((fail_count + 1))
            fi
        fi

        rm $asm_file $output_file
    fi

    test_no=$((test_no + 1))
done

rm -f *.out *.ssa *.s
echo

echo "================ test result ==============="
echo "total/failed/passed/other: $total_tests/$fail_count/$pass_count/$((total_tests - fail_count - pass_count))"
