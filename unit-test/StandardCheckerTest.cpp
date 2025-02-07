#include "env.hpp"
#include "gtest/gtest.h"
#include "judge/programming.hpp"
#include "test/mock_judge_server.hpp"
#include "test/worker.hpp"

using namespace std;
using namespace std::filesystem;
using namespace judge;

class StandardCheckerTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        setup_test_environment();
    }

    static void TearDownTestCase() {
    }

    void prepare(programming_submission &prog, const string &source) {
        prog.category = "mock";
        prog.prob_id = "1234";
        prog.sub_id = "12340";
        prog.updated_at = chrono::system_clock::to_time_t(chrono::system_clock::now());
        auto submission = make_unique<source_code>();
        submission->language = "cpp";
        submission->source_files.push_back(make_unique<text_asset>("main.cpp", source));
        prog.submission = move(submission);

        {
            test_case_data data;
            data.inputs.push_back(make_unique<text_asset>("testdata.in", "1"));
            data.outputs.push_back(make_unique<text_asset>("testdata.out", "1"));
            prog.test_data.push_back(move(data));
        }

        {
            test_case_data data;
            data.inputs.push_back(make_unique<text_asset>("testdata.in", "2"));
            data.outputs.push_back(make_unique<text_asset>("testdata.out", "2"));
            prog.test_data.push_back(move(data));
        }

        {  // 编译测试
            judge_task testcase;
            testcase.check_script = "compile";
            prog.judge_tasks.push_back(testcase);
        }

        {  // 标准测试
            judge_task testcase;
            testcase.depends_on = 0;  // 依赖编译任务
            testcase.depends_cond = judge_task::dependency_condition::ACCEPTED;
            testcase.check_script = "standard-trusted";
            testcase.run_script = "standard";
            testcase.compare_script = "diff-all";
            testcase.time_limit = 1;
            testcase.memory_limit = 32768;
            testcase.file_limit = 32768;
            testcase.proc_limit = -1;

            for (size_t i = 0; i < 2; ++i) {
                testcase.testcase_id = i;
                prog.judge_tasks.push_back(testcase);
            }
        }
    }
};

TEST_F(StandardCheckerTest, CompilationTimeLimitTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(#include </dev/random>
int main() {
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::COMPILATION_ERROR);
    EXPECT_EQ(prog.results[1].status, status::DEPENDENCY_NOT_SATISFIED);
    EXPECT_EQ(prog.results[2].status, status::DEPENDENCY_NOT_SATISFIED);
}

TEST_F(StandardCheckerTest, AcceptedTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(#include <iostream>
int main() {
    int a;
    std::cin >> a;
    std::cout << a;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[2].status, status::ACCEPTED);
}

TEST_F(StandardCheckerTest, WrongAnswerTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(#include <iostream>
int main () {
    int a;
    std::cin >> a;
    std::cout << a + 1;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::WRONG_ANSWER);
    EXPECT_EQ(prog.results[2].status, status::WRONG_ANSWER);
}

TEST_F(StandardCheckerTest, PresentationErrorTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(#include <iostream>
int main () {
    int a;
    std::cin >> a;
    std::cout << a << std::endl;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::PRESENTATION_ERROR);
    EXPECT_EQ(prog.results[2].status, status::PRESENTATION_ERROR);
}

TEST_F(StandardCheckerTest, CompilationErrorTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(
int main() {
    cin >> a;
    cout << a;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::COMPILATION_ERROR);
    EXPECT_EQ(prog.results[1].status, status::DEPENDENCY_NOT_SATISFIED);
    EXPECT_EQ(prog.results[2].status, status::DEPENDENCY_NOT_SATISFIED);
}

TEST_F(StandardCheckerTest, TimeLimitExceededTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(
int main () {
    int a;
    while (1) a++;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::TIME_LIMIT_EXCEEDED);
    EXPECT_EQ(prog.results[2].status, status::TIME_LIMIT_EXCEEDED);
}

TEST_F(StandardCheckerTest, MemoryLimitExceededTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(#include <iostream>
int dp[5000][5000];
int main () {
    for (int i = 1; i < 5000; ++i)
        for (int j = 1; j < 5000; ++j)
            dp[i][j] = dp[i - 1][j] + dp[i][j - 1] - dp[i - 1][j - 1];
    std::cout << dp[4999][4999];
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::MEMORY_LIMIT_EXCEEDED);
    EXPECT_EQ(prog.results[2].status, status::MEMORY_LIMIT_EXCEEDED);
}

TEST_F(StandardCheckerTest, FloatingPointErrorTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(#include <iostream>
using namespace std;
int main() {
    int a = time(NULL) * 1, b = time(NULL) - a;
    printf("%d\n", a / b);
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::FLOATING_POINT_ERROR);
    EXPECT_EQ(prog.results[2].status, status::FLOATING_POINT_ERROR);
}

TEST_F(StandardCheckerTest, SegmentationFaultTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(#include <iostream>
int main() {
    int *a = nullptr;
    std::cin >> *a;
    std::cout << *a;
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::SEGMENTATION_FAULT);
    EXPECT_EQ(prog.results[2].status, status::SEGMENTATION_FAULT);
}

TEST_F(StandardCheckerTest, RuntimeErrorTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(#include <stdexcept>
int main() {
    throw std::invalid_argument("???");
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::RUNTIME_ERROR);
    EXPECT_EQ(prog.results[2].status, status::RUNTIME_ERROR);
}

TEST_F(StandardCheckerTest, RestrictFunctionPassTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(#include <cstdlib>
int main() {
    system("echo 1");
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::PRESENTATION_ERROR);
    EXPECT_EQ(prog.results[2].status, status::WRONG_ANSWER);
}

TEST_F(StandardCheckerTest, RestrictFunctionFailTest) {
    concurrent_queue<message::client_task> task_queue;
    judge::server::mock::configuration mock_judge_server;
    programming_submission prog;
    prog.judge_server = &mock_judge_server;
    prepare(prog, R"(#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    return 0;
})");
    programming_judger judger;

    push_submission(judger, task_queue, prog);
    worker_loop(judger, task_queue);

    EXPECT_EQ(prog.results[0].status, status::ACCEPTED);
    EXPECT_EQ(prog.results[1].status, status::RESTRICT_FUNCTION);
    EXPECT_EQ(prog.results[2].status, status::RESTRICT_FUNCTION);
}
