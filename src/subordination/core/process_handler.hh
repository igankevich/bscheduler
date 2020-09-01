#ifndef SUBORDINATION_CORE_PROCESS_HANDLER_HH
#define SUBORDINATION_CORE_PROCESS_HANDLER_HH

#include <cassert>
#include <iosfwd>

#include <unistdx/io/fildes_pair>
#include <unistdx/io/poller>
#include <unistdx/ipc/process>

#include <subordination/core/application.hh>
#include <subordination/core/connection.hh>
#include <subordination/core/pipeline_base.hh>

namespace sbn {

    class process_handler: public connection {

    private:
        enum class role_type {child, parent};

    private:
        sys::pid_type _child_process_id;
        sys::fildes_pair _file_descriptors;
        ::sbn::application _application;
        role_type _role;
        int _kernel_count = 0;
        int _kernel_count_last = 0;
        time_point _last{};
        foreign_kernel_ptr _main_kernel{};
        pipeline* _unix{};

    public:

        /// Called from parent process.
        process_handler(sys::pid_type&& child,
                        sys::two_way_pipe&& pipe,
                        const ::sbn::application& app);

        /// Called from child process.
        explicit process_handler(sys::pipe&& pipe);

        virtual
        ~process_handler() {
            // recover kernels from upstream and downstream buffer
            recover_kernels(true);
        }

        void handle(const sys::epoll_event& event) override;
        void add(const connection_ptr& self) override;
        void remove(const connection_ptr& self) override;
        void flush() override;
        void stop() override;

        inline void forward(foreign_kernel_ptr&& k) {
            // remove target application before forwarding
            // to child process to reduce the amount of data
            // transferred to child process
            bool wait_for_completion = false;
            if (auto* a = k->target_application()) {
                wait_for_completion = a->wait_for_completion();
                if (k->source_application_id() == a->id()) {
                    k->target_application_id(a->id());
                }
            }
            // save the main kernel
            k = connection::do_forward(std::move(k));
            if (k) {
                if (k->type_id() == 1) {
                    if (wait_for_completion) {
                        log("save main kernel _", *k);
                        this->_main_kernel = std::move(k);
                    } else {
                        log("return main kernel _", *k);
                        k->return_to_parent();
                        k->target_application_id(0);
                        k->source_application_id(application().id());
                        parent()->forward_to(this->_unix, std::move(k));
                    }
                }
            }
            ++this->_kernel_count;
        }

        inline const ::sbn::application& application() const noexcept {
            return this->_application;
        }

        inline sys::pid_type child_process_id() const noexcept {
            return this->_child_process_id;
        }

        inline bool stale(time_point now, duration timeout) noexcept {
            if (now-this->_last < timeout) { return false; }
            bool changed = this->_kernel_count != this->_kernel_count_last;
            this->_kernel_count_last = this->_kernel_count;
            this->_last = now;
            return !changed;
        }

        inline sys::fd_type in() const noexcept { return this->_file_descriptors.in().fd(); }
        inline sys::fd_type out() const noexcept { return this->_file_descriptors.out().fd(); }
        inline pipeline* unix() const noexcept { return this->_unix; }
        inline void unix(pipeline* rhs) noexcept { this->_unix = rhs; }

    protected:
        void receive_foreign_kernel(foreign_kernel_ptr&& fk) override;

    };

}

#endif // vim:filetype=cpp