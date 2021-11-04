# be careful, this cleans, prepares and runs all tests
# discards all output of clean and prepare, use only if setups is proven to work

scripts/cleanup_tests.sh 2>/dev/null >/dev/null
scripts/prepare_tests.sh 2>/dev/null >/dev/null
./run_tests.sh
