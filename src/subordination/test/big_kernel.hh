#ifndef SUBORDINATION_TEST_BIG_KERNEL_HH
#define SUBORDINATION_TEST_BIG_KERNEL_HH

#include <subordination/api.hh>
#include <subordination/test/datum.hh>

template<uint32_t Size>
struct Big_kernel: public sbn::kernel {

    Big_kernel():
    _data(Size)
    {}

    virtual ~Big_kernel() = default;

    void act() override {
        sbn::commit<sbn::Remote>(this);
    }

    void
    write(sbn::kernel_buffer& out) const override {
        sbn::kernel::write(out);
        out << uint32_t(_data.size());
        for (size_t i=0; i<_data.size(); ++i)
            out << _data[i];
    }

    void
    read(sbn::kernel_buffer& in) override {
        sbn::kernel::read(in);
        uint32_t sz;
        in >> sz;
        _data.resize(sz);
        for (size_t i=0; i<_data.size(); ++i)
            in >> _data[i];
    }

    std::vector<Datum>
    data() const noexcept {
        return _data;
    }

    bool
    operator==(const Big_kernel& rhs) const noexcept {
        return _data.size() == rhs._data.size()
            and std::equal(_data.begin(), _data.end(),
                rhs._data.begin());
    }

    bool
    operator!=(const Big_kernel& rhs) const noexcept {
        return !operator==(rhs);
    }

    friend std::ostream&
    operator<<(std::ostream& out, const Big_kernel& rhs) {
        for (const Datum& x : rhs._data) {
            out << sys::make_bytes(x);
        }
        return out;
    }

private:

    std::vector<Datum> _data;

};

#endif // vim:filetype=cpp
