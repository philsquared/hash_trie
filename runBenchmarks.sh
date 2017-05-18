cmake-build-release/HamtBench -s 100 -r html -f ".*<int>::insert" -o "BenchmarkResults/insert_hash_e5.html" -i 100000


cmake-build-release/HamtBench -s 1000 -r html -f ".*<hash>::find" -o "BenchmarkResults/find_hash_e2.html" -i 100
cmake-build-release/HamtBench -s 1000 -r html -f ".*<int>::find" -o "BenchmarkResults/find_int_e2.html" -i 100
cmake-build-release/HamtBench -s 1000 -r html -f ".*<string>::find" -o "BenchmarkResults/find_string_e2.html" -i 100

cmake-build-release/HamtBench -s 1000 -r html -f ".*<hash>::find" -o "BenchmarkResults/find_hash_e3.html" -i 1000
cmake-build-release/HamtBench -s 1000 -r html -f ".*<int>::find" -o "BenchmarkResults/find_int_e3.html" -i 1000
cmake-build-release/HamtBench -s 1000 -r html -f ".*<string>::find" -o "BenchmarkResults/find_string_e3.html" -i 1000

cmake-build-release/HamtBench -s 1000 -r html -f ".*<hash>::find" -o "BenchmarkResults/find_hash_e4.html" -i 10000
cmake-build-release/HamtBench -s 1000 -r html -f ".*<int>::find" -o "BenchmarkResults/find_int_e4.html" -i 10000
cmake-build-release/HamtBench -s 1000 -r html -f ".*<string>::find" -o "BenchmarkResults/find_string_e4.html" -i 10000

cmake-build-release/HamtBench -s 100 -r html -f ".*<hash>::find" -o "BenchmarkResults/find_hash_e5.html" -i 100000
cmake-build-release/HamtBench -s 100 -r html -f ".*<int>::find" -o "BenchmarkResults/find_int_e5.html" -i 100000
cmake-build-release/HamtBench -s 100 -r html -f ".*<string>::find" -o "BenchmarkResults/find_string_e5.html" -i 100000

cmake-build-release/HamtBench -s 100 -r html -f ".*<hash>::find" -o "BenchmarkResults/find_hash_e6.html" -i 1000000
cmake-build-release/HamtBench -s 100 -r html -f ".*<int>::find" -o "BenchmarkResults/find_int_e6.html" -i 1000000
cmake-build-release/HamtBench -s 100 -r html -f ".*<string>::find" -o "BenchmarkResults/find_string_e6.html" -i 1000000
