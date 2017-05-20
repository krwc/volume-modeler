#include "gtest/gtest.h"

#include "compute/context.h"
#include "compute/scan.h"

#include <vector>

struct TestCtx {
    compute::context context;
    compute::command_queue queue;

    TestCtx() {
        compute::device gpu = compute::system::default_device();
        context = std::move(compute::context(gpu));
        queue = std::move(compute::command_queue(context, gpu));
    }
};

TEST(scan, inclusive_big) {
    TestCtx ctx{};

    std::vector<uint32_t> numbers(80 * 80 * 80 * 4, 0);
    std::vector<uint32_t> cpu_scan_result(numbers.size());
    for (size_t i = 0; i < numbers.size(); ++i) {
        numbers[i] = i % 8;
    }
    cpu_scan_result = numbers;
    for (size_t i = 1; i < numbers.size(); ++i) {
        cpu_scan_result[i] += cpu_scan_result[i - 1];
    }

    vm::compute::Scan gpu_scan(ctx.queue, numbers.size());
    compute::vector<uint32_t> input(numbers.size(), ctx.context);
    compute::vector<uint32_t> output(numbers.size(), ctx.context);
    compute::copy(numbers.begin(), numbers.end(), input.begin(), ctx.queue);

    gpu_scan.inclusive_scan(input, output, ctx.queue).wait();

    std::vector<uint32_t> gpu_scan_result(numbers.size(), 0);
    compute::copy(output.begin(), output.end(), gpu_scan_result.begin(), ctx.queue);

    for (size_t i = 0; i < numbers.size(); ++i) {
        ASSERT_EQ(cpu_scan_result[i], gpu_scan_result[i]);
    }
}

TEST(scan, inclusive_small) {
    TestCtx ctx{};

    std::vector<uint32_t> numbers(1024, 0);
    std::vector<uint32_t> cpu_scan_result(numbers.size());
    for (size_t i = 0; i < numbers.size(); ++i) {
        numbers[i] = i % 8;
    }
    cpu_scan_result = numbers;
    for (size_t i = 1; i < numbers.size(); ++i) {
        cpu_scan_result[i] += cpu_scan_result[i - 1];
    }

    vm::compute::Scan gpu_scan(ctx.queue, numbers.size());
    compute::vector<uint32_t> input(numbers.size(), ctx.context);
    compute::vector<uint32_t> output(numbers.size(), ctx.context);
    compute::copy(numbers.begin(), numbers.end(), input.begin(), ctx.queue);

    gpu_scan.inclusive_scan(input, output, ctx.queue).wait();

    std::vector<uint32_t> gpu_scan_result(numbers.size(), 0);
    compute::copy(output.begin(), output.end(), gpu_scan_result.begin(), ctx.queue);

    for (size_t i = 0; i < numbers.size(); ++i) {
        ASSERT_EQ(cpu_scan_result[i], gpu_scan_result[i]);
    }
}

TEST(scan, inplace) {
    TestCtx ctx{};

    std::vector<uint32_t> numbers(64 * 64 * 64, 0);
    std::vector<uint32_t> cpu_scan_result(numbers.size());
    for (size_t i = 0; i < numbers.size(); ++i) {
        numbers[i] = i % 8;
    }
    cpu_scan_result = numbers;
    for (size_t i = 1; i < numbers.size(); ++i) {
        cpu_scan_result[i] += cpu_scan_result[i - 1];
    }

    vm::compute::Scan gpu_scan(ctx.queue, numbers.size());
    compute::vector<uint32_t> input(numbers.size(), ctx.context);
    compute::copy(numbers.begin(), numbers.end(), input.begin(), ctx.queue);

    gpu_scan.inclusive_scan(input, input, ctx.queue).wait();

    std::vector<uint32_t> gpu_scan_result(numbers.size(), 0);
    compute::copy(input.begin(), input.end(), gpu_scan_result.begin(), ctx.queue);

    for (size_t i = 0; i < numbers.size(); ++i) {
        ASSERT_EQ(cpu_scan_result[i], gpu_scan_result[i]);
    }
}

TEST(scan, invalid_input_size) {
    TestCtx ctx{};
    ASSERT_THROW(vm::compute::Scan gpu_scan(ctx.queue, 1023),
                 std::invalid_argument);
}
