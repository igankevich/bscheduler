#ifndef SUBORDINATION_KERNEL_KSTREAM_HH
#define SUBORDINATION_KERNEL_KSTREAM_HH

#include <cassert>

#include <unistdx/base/log_message>
#include <unistdx/net/pstream>
#include <unistdx/net/socket_address>

#include <subordination/base/error.hh>
#include <subordination/kernel/foreign_kernel.hh>
#include <subordination/kernel/kernel_error.hh>
#include <subordination/kernel/kernel_type_registry.hh>
#include <subordination/kernel/kernelbuf.hh>
#include <subordination/ppl/kernel_proto_flag.hh>

namespace sbn {

    struct kstream: public sys::pstream {

        using sys::pstream::operator<<;
        using sys::pstream::operator>>;

        kstream() = default;
        inline explicit
        kstream(sys::packetbuf* buf): sys::pstream(buf) {}
        kstream(kstream&&) = default;

        inline kstream&
        operator<<(foreign_kernel* k) {
            return operator<<(*k);
        }

        inline kstream&
        operator<<(foreign_kernel& k) {
            this->write_foreign(k);
            return *this;
        }

        inline kstream&
        operator<<(kernel* k) {
            return operator<<(*k);
        }

        kstream&
        operator<<(kernel& k) {
            this->write_native(k);
            if (k.carries_parent()) {
                // embed parent into the packet
                auto* parent = k.parent();
                if (!parent) {
                    throw std::invalid_argument("parent is null");
                }
                this->write_native(*parent);
            }
            return *this;
        }

        kstream&
        operator>>(kernel*& k) {
            k = this->read_native();
            if (k->carries_parent()) {
                auto* parent = this->read_native();
                k->parent(parent);
            }
            return *this;
        }

        kstream&
        operator>>(foreign_kernel& k) {
            this->read_foreign(k);
            return *this;
        }

    private:

        inline void
        write_native(kernel& k) {
            auto type = types.find(typeid(k));
            if (type == types.end()) {
                throw std::invalid_argument("kernel type is null");
            }
            *this << type->id();
            k.write(*this);
        }

        inline kernel*
        read_native() {
            return types.read_object(*this);
        }

        inline void
        write_foreign(foreign_kernel& k) {
            k.write(*this);
        }

        inline void
        read_foreign(foreign_kernel& k) {
            k.read(*this);
        }

    };

}

#endif // vim:filetype=cpp
