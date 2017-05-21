#include "gtest/gtest.h"

#include "compute/context.h"
#include "compute/scan.h"

#include <vector>

struct TestSuite {
    compute::context context;
    compute::command_queue queue;

    std::vector<uint32_t> numbers;
    std::vector<uint32_t> cpu_scan_result;
    std::vector<uint32_t> gpu_scan_result;
    compute::vector<uint32_t> input;
    compute::vector<uint32_t> output;
    vm::compute::Scan gpu_scan;

    TestSuite(size_t input_size, bool in_place = false)
        : context(compute::system::default_device())
        , queue(context,
                compute::system::default_device())
        , numbers(input_size, 0)
        , cpu_scan_result(input_size)
        , gpu_scan_result(input_size)
        , input(input_size, context)
        , output(input_size, context)
        , gpu_scan(queue, input_size) {

        for (size_t i = 0; i < numbers.size(); ++i) {
            numbers[i] = i % 8;
        }

        cpu_scan_result = numbers;
        for (size_t i = 1; i < numbers.size(); ++i) {
            cpu_scan_result[i] += cpu_scan_result[i - 1];
        }
        compute::copy(numbers.begin(), numbers.end(), input.begin(), queue);

        if (in_place) {
            gpu_scan.inclusive_scan(input, input, queue);
            compute::copy(input.begin(), input.end(), gpu_scan_result.begin(),
                          queue);
        } else {
            gpu_scan.inclusive_scan(input, output, queue);
            compute::copy(output.begin(), output.end(), gpu_scan_result.begin(),
                          queue);
        }
        for (size_t i = 0; i < numbers.size(); ++i) {
            if (cpu_scan_result[i] != gpu_scan_result[i]) {
                throw std::runtime_error("Mismatch at index " + std::to_string(i));
            }
        }
    }
};

TEST(scan, inclusive_big) {
    TestSuite{ 80 * 80 * 80 * 4, false };
}

TEST(scan, inclusive_small) {
    TestSuite{ 1024, false };
}

TEST(scan, inplace) {
    TestSuite{ 64 * 64 * 64, true };
}

TEST(scan, non_aligned_size_small) {
    TestSuite{1023, false};
}

TEST(scan, non_aligned_size_big) {
    TestSuite{ 80 * 80 * 80 * 4 + 1, false };
}

TEST(scan, non_aligned_inplace) {
    TestSuite{ 64 * 64 * 64 + 1, true };
}
