#include "gtest/gtest.h"

#include "compute/context.h"
#include "compute/scan.h"

#include <chrono>
#include <vector>

namespace {
struct TestContext {
    std::vector<uint32_t> numbers;
    std::vector<uint32_t> cpu_scan_result;
    std::vector<uint32_t> gpu_scan_result;
    compute::vector<uint32_t> input;
    compute::vector<uint32_t> output;
    vm::Scan gpu_scan;

    TestContext(compute::context &context,
                compute::command_queue &queue,
                size_t input_size)
            : numbers(input_size, 0)
            , cpu_scan_result(input_size)
            , gpu_scan_result(input_size)
            , input(input_size, context)
            , output(input_size, context)
            , gpu_scan(context) {

        for (size_t i = 0; i < numbers.size(); ++i) {
            numbers[i] = i % 8;
        }

        cpu_scan_result = numbers;
        for (size_t i = 1; i < numbers.size(); ++i) {
            cpu_scan_result[i] += cpu_scan_result[i - 1];
        }
        compute::copy(numbers.begin(), numbers.end(), input.begin(), queue);
    }
};

struct TestSuite {
    compute::context context;
    compute::command_queue queue;

    TestContext test;

    TestSuite(size_t input_size, bool in_place = false)
            : context(compute::system::default_device())
            , queue(context, compute::system::default_device())
            , test(context, queue, input_size) {

        if (in_place) {
            test.gpu_scan.inclusive_scan(test.input, test.input, queue);
            compute::copy(test.input.begin(),
                          test.input.end(),
                          test.gpu_scan_result.begin(),
                          queue);
        } else {
            test.gpu_scan.inclusive_scan(test.input, test.output, queue);
            compute::copy(test.output.begin(),
                          test.output.end(),
                          test.gpu_scan_result.begin(),
                          queue);
        }
        for (size_t i = 0; i < test.numbers.size(); ++i) {
            if (test.cpu_scan_result[i] != test.gpu_scan_result[i]) {
                throw std::runtime_error("Mismatch at index "
                                         + std::to_string(i));
            }
        }
    }
};
} // namespace

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
    TestSuite{ 1023, false };
}

TEST(scan, non_aligned_size_big) {
    TestSuite{ 80 * 80 * 80 * 4 + 1, false };
}

TEST(scan, non_aligned_inplace) {
    TestSuite{ 64 * 64 * 64 + 1, true };
}

TEST(scan, different_small_sizes) {
    std::vector<size_t> sizes{ 1,
                               2,
                               512,
                               1023,
                               1024,
                               1025,
                               16 * 16 * 16,
                               18 * 18 * 18,
                               21 * 21 * 21,
                               40 * 40 * 40 };
    for (size_t size : sizes) {
        TestSuite{ size, false };
        TestSuite{ size, true };
    }
}

TEST(scan, different_big_sizes) {
    std::vector<size_t> sizes{ 41 * 41 * 41, 80 * 80 * 80, 81 * 81 * 81,
                               83 * 83 * 83, 84 * 84 * 84, 96 * 96 * 96,
                               98 * 98 * 98 };
    for (size_t size : sizes) {
        TestSuite{ size, false };
        TestSuite{ size, true };
    }
}

TEST(scan, performance) {
    compute::device gpu = compute::system::default_device();
    compute::context context(gpu);
    compute::command_queue queue(context, gpu);

    std::vector<size_t> sizes{ 1024,         2048,         4096,
                               32 * 32 * 32, 64 * 64 * 64, 80 * 80 * 80 };

    for (size_t size : sizes) {
        TestContext test(context, queue, size);
        const size_t NUM_TESTS = 4096;
        double total_time = 0;
        double max_time = 0;
        double min_time = 1e9;
        std::vector<double> dts(NUM_TESTS);

        for (size_t i = 0; i < NUM_TESTS; ++i) {
            auto t0 = std::chrono::steady_clock::now();
            test.gpu_scan.inclusive_scan(test.input, test.output, queue);
            queue.flush();
            queue.finish();
            auto t1 = std::chrono::steady_clock::now();
            double dt = std::chrono::duration_cast<std::chrono::microseconds>(
                                t1 - t0)
                                .count();
            dts[i] = dt;
            total_time += dt;
            max_time = std::max(max_time, dt);
            min_time = std::min(min_time, dt);
        }
        double stddev = 0;
        for (double dt : dts) {
            stddev += std::pow(dt - (total_time / NUM_TESTS), 2);
        }
        stddev = std::sqrt(stddev / NUM_TESTS);
        std::cerr << "Stats for " << size << " elements: " << std::endl
                  << "Total time : " << total_time << "us" << std::endl
                  << "Avg time   : " << (total_time / NUM_TESTS) << "us"
                  << std::endl
                  << "Stddev     : " << stddev << std::endl
                  << "Min time   : " << min_time << "us" << std::endl
                  << "Max time   : " << max_time << "us" << std::endl
                  << std::endl;
    }
}
