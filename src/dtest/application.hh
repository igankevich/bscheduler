#ifndef DTEST_APPLICATION_HH
#define DTEST_APPLICATION_HH

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <queue>
#include <thread>

#include <unistdx/base/byte_buffer>
#include <unistdx/base/log_message>
#include <unistdx/io/poller>
#include <unistdx/ipc/argstream>
#include <unistdx/ipc/process_group>

#include <dtest/cluster.hh>
#include <dtest/cluster_node.hh>
#include <dtest/cluster_node_bitmap.hh>
#include <dtest/exit_code.hh>

namespace dts {

    class application;

    class process_output {

    public:
        using line_array = std::vector<std::string>;

    private:
        sys::byte_buffer _buffer;
        std::string _prefix;
        sys::fildes _in;
        sys::fd_type _out;

    public:

        inline explicit
        process_output(
            const std::string& prefix,
            sys::fildes&& in,
            sys::fd_type out,
            size_t size=4096
        ):
        _buffer{size}, _prefix(prefix), _in(std::move(in)), _out(out) {}

        void copy(line_array& lines);

        inline const sys::fildes& in() const { return this->_in; }
        inline const sys::fd_type& out() const { return this->_out; }

    };

    class test {

    public:
        using line_array = process_output::line_array;
        using test_function = std::function<void(application&, const line_array&)>;

    private:
        std::string _description;
        test_function _function;

    public:
        inline explicit test(std::string d, test_function f): _description(d), _function(f) {}
        test() = default;
        ~test() = default;
        test(const test&) = default;
        test& operator=(const test&) = default;
        test(test&&) = default;
        test& operator=(test&&) = default;

        inline const std::string& description() const noexcept {
            return this->_description;
        }

        inline void description(std::string rhs) { this->_description = std::move(rhs); }

        inline void function(const test_function& rhs) noexcept {
            this->_function = rhs;
        }

        inline void operator()(application& a, const line_array& lines) {
            this->_function(a, lines);
        }

    };

    class application {

    public:
        using line_array = process_output::line_array;
        using test_queue = std::queue<test>;
        using arguments_array = std::vector<sys::argstream>;

    private:
        using duration = std::chrono::system_clock::duration;
        using exit_code_type = ::dts::exit_code;

    private:
        ::dts::cluster _cluster;
        arguments_array _arguments;
        std::vector<cluster_node_bitmap> _where;
        sys::process_group _child_processes;
        std::vector<process_output> _output;
        sys::event_poller _poller;
        std::thread _output_thread;
        exit_code_type _exit_code = exit_code_type::all;
        duration _execution_delay = duration::zero();
        char** _argv = nullptr;
        bool _will_restart = false;
        std::atomic<bool> _stopped{false};
        test_queue _tests;
        line_array _lines;
        bool _no_tests = false;
        bool _tests_succeeded = false;
        std::promise<void> _tests_completed;

    public:

        application() = default;
        inline explicit application(int argc, char* argv[]) { this->init(argc, argv); }

        void usage();
        void init(int argc, char* argv[]);
        void run();
        int wait();

        inline void send(sys::signal s) { this->_child_processes.send(s); }
        inline void terminate() { this->send(sys::signal::terminate); }
        inline bool stopped() { return this->_stopped; }
        inline bool will_restart() const noexcept { return this->_will_restart; }
        inline void will_restart(bool rhs) noexcept { this->_will_restart = rhs; }
        inline void cluster(::dts::cluster&& rhs) { this->_cluster = std::move(rhs); }
        inline const ::dts::cluster& cluster() const noexcept { return this->_cluster; }
        inline void arguments(arguments_array&& rhs) { this->_arguments = std::move(rhs); }
        inline const arguments_array& arguments() const noexcept { return this->_arguments; }
        inline void exit_code(exit_code_type rhs) noexcept { this->_exit_code = rhs; }
        inline exit_code_type exit_code() const noexcept { return this->_exit_code; }
        inline void execution_delay(duration rhs) noexcept { this->_execution_delay = rhs; }
        inline duration execution_delay() const noexcept { return this->_execution_delay; }
        void add_process(cluster_node_bitmap nodes, sys::argstream args);
        void run_process(cluster_node_bitmap where, sys::argstream args);

        inline void add_test(test t) { this->_tests.emplace(std::move(t)); }
        template <class ... Args>
        inline void emplace_test(Args&& ... args) {
            this->_tests.emplace(std::forward<Args>(args)...);
        }

        template <class ... Args>
        inline void
        log(const Args& ... args) const {
            sys::log_message("dtest", args...);
        }

    private:

        int accumulate_return_value();
        void process_events();
        bool run_tests();

    };

    int run(application& app);

}

#endif // vim:filetype=cpp
