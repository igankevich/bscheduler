#include <subordination/daemon/hierarchy_node.hh>

#include <unistdx/base/make_object>

std::ostream&
sbn::operator<<(std::ostream& out, const hierarchy_node& rhs) {
    return out << sys::make_object(
        "socket_address", rhs.socket_address(),
        "weight", rhs.weight()
    );
}