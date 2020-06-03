#ifndef SUBORDINATION_PPL_PROCESS_HANDLER_HH
#define SUBORDINATION_PPL_PROCESS_HANDLER_HH

#include <cassert>
#include <iosfwd>

#include <unistdx/io/fildes_pair>
#include <unistdx/io/fildesbuf>
#include <unistdx/io/poller>
#include <unistdx/ipc/process>

#include <subordination/kernel/kstream.hh>
#include <subordination/ppl/application.hh>
#include <subordination/ppl/basic_handler.hh>
#include <subordination/ppl/kernel_protocol.hh>
#include <subordination/ppl/pipeline_base.hh>

namespace sbn {

    class process_handler: public basic_handler {

    private:
        typedef sys::basic_fildesbuf<char, std::char_traits<char>, sys::fildes_pair>
            fildesbuf_type;
        typedef basic_kernelbuf<fildesbuf_type> kernelbuf_type;
        typedef std::unique_ptr<kernelbuf_type> kernelbuf_ptr;
        typedef kstream stream_type;
        typedef kernel_protocol protocol_type;

        enum class role_type {
            child,
            parent
        };

    private:
        sys::pid_type _childpid;
        kernelbuf_ptr _packetbuf;
        stream_type _stream;
        protocol_type _proto;
        application _application;
        role_type _role;

    public:

        /// Called from parent process.
        process_handler(sys::pid_type&& child,
                        sys::two_way_pipe&& pipe,
                        const application& app);

        /// Called from child process.
        explicit process_handler(sys::pipe&& pipe);

        virtual
        ~process_handler() {
            // recover kernels from upstream and downstream buffer
            this->_proto.recover_kernels(true);
        }

        const sys::pid_type&
        childpid() const {
            return this->_childpid;
        }

        const application&
        app() const noexcept {
            return this->_application;
        }

        void
        close() {
            this->_packetbuf->fd().close();
        }

        void
        send(kernel* k) {
            this->_proto.send(k, this->_stream, sys::socket_address{});
        }

        void
        handle(const sys::epoll_event& event) override;

        void
        flush() override {
            if (this->_packetbuf->dirty()) {
                this->_packetbuf->pubflush();
            }
        }

        void
        write(std::ostream& out) const override;

        void
        remove(sys::event_poller& poller) override;

        void
        forward(foreign_kernel* k) {
            // remove application before forwarding
            // to child process
            k->aptr(nullptr);
            this->_proto.forward(k, this->_stream);
        }

        inline void
        set_name(const char* rhs) noexcept {
            this->pipeline_base::set_name(rhs);
            this->_proto.set_name(rhs);
            #ifndef NDEBUG
            if (this->_packetbuf) {
                this->_packetbuf->set_name(rhs);
            }
            #endif
        }

        inline sys::fd_type
        in() const noexcept {
            return this->_packetbuf->fd().in().fd();
        }

        inline sys::fd_type
        out() const noexcept {
            return this->_packetbuf->fd().out().fd();
        }

    };

}

#endif // vim:filetype=cpp
