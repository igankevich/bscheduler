#ifndef SYSX_FILDESBUF_HH
#define SYSX_FILDESBUF_HH

#include <vector>
#include <cassert>

#include <sysx/bits/buffer_category.hh>
#include <sysx/fildes.hh>
#include <stdx/packetbuf.hh>
#include <stdx/log.hh>

namespace sysx {

	template<class Ch, class Tr=std::char_traits<Ch>, class Fd=sysx::fildes>
	struct basic_fildesbuf: public stdx::basic_packetbuf<Ch,Tr> {

		typedef stdx::basic_packetbuf<Ch,Tr> base_type;
		using base_type::gptr;
		using base_type::eback;
		using base_type::egptr;
		using base_type::pptr;
		using base_type::pbase;
		using base_type::epptr;
		using base_type::setg;
		using base_type::setp;
		using base_type::gbump;
		using base_type::pbump;
		using typename base_type::int_type;
		using typename base_type::traits_type;
		using typename base_type::char_type;
		using typename base_type::pos_type;
		using typename base_type::off_type;
		typedef std::ios_base::openmode openmode;
		typedef std::ios_base::seekdir seekdir;
		typedef Fd fd_type;
		typedef stdx::log<basic_fildesbuf> this_log;
		typedef typename std::vector<char_type>::size_type size_type;
		typedef typename std::make_signed<size_type>::type signed_size_type;

		basic_fildesbuf(): basic_fildesbuf(std::move(fd_type()), 512, 512) {}

		explicit
		basic_fildesbuf(fd_type&& fd): basic_fildesbuf(std::move(fd), 512, 512) {}

		basic_fildesbuf(fd_type&& fd, size_type gbufsize, size_type pbufsize):
		_fd(std::move(fd)), _gbuf(gbufsize), _pbuf(pbufsize)
		{
			char_type* end = _gbuf.data();
			setg(end, end, end);
			char_type* beg = _pbuf.data();
			setp(beg, beg + _pbuf.size());
		}

		basic_fildesbuf(basic_fildesbuf&& rhs) = default;

		virtual
		~basic_fildesbuf() {
			// NB: for now full sync is not guaranteed for non-blocking I/O
			// as it may result in infinite loop
			sync();
		}

		int_type
		underflow() override {
			assert(gptr() == egptr());
			setg(eback(), eback(), eback());
			return do_fill() == 0
				? traits_type::eof()
				: traits_type::to_int_type(*gptr());
		}

		int_type
		overflow(int_type c) override {
			assert(pptr() == epptr());
			if (c != traits_type::eof()) {
				pgrow();
				if (pptr() != epptr()) {
					*pptr() = c;
					pbump(1);
				}
			} else {
				sync();
			}
			return c;
		}

		std::streamsize
		fill() override {
			return do_fill();
		}

		int
		sync() override {
			const std::streamsize m = pptr() - pbase();
			const std::streamsize n = _fd.write(pbase(), m);
			if (n > 0) {
				pbump(-n);
			}
			return n == m ? 0 : -1;
		}

		pos_type
		seekoff(off_type off, seekdir way, openmode which) override {
			pos_type ret(off_type(-1));
			if (way == std::ios_base::beg) {
				this_log() << "seekoff way=beg,off=" << off << std::endl;
				ret = seekpos(off, which);
			}
			if (way == std::ios_base::cur) {
				this_log() << "seekoff way=cur,off=" << off << std::endl;
				const pos_type pos = which & std::ios_base::in
					? static_cast<pos_type>(gptr() - eback())
					: static_cast<pos_type>(pptr() - pbase());
				ret = off == 0 ? pos : seekpos(pos + off, which);
			}
			if (way == std::ios_base::end) {
				this_log() << "seekoff way=end,off=" << off << std::endl;
				const pos_type pos = which & std::ios_base::in
					? static_cast<pos_type>(egptr() - eback())
					: static_cast<pos_type>(epptr() - pbase());
				ret = seekpos(pos + off, which);
			}
			return ret;
		}

		pos_type
		seekpos(pos_type pos, openmode mode) override {
			pos_type ret(off_type(-1));
			if (mode & std::ios_base::in) {
				const std::streamsize size = egptr() - eback();
				if (pos >= 0 && pos <= size) {
					setg(eback(), eback()+pos, egptr());
					ret = pos;
				}
			}
			if (mode & std::ios_base::out) {
				if (pos >= 0 && pos <= psize()) {
					setp(pbase(), epptr());
					pbump(pos);
					ret = pos;
				}
			}
			return ret;
		}

		void
		setfd(fd_type&& rhs) {
			_fd = std::move(rhs);
		}

		const fd_type&
		fd() const {
			return _fd;
		}

		fd_type&
		fd() {
			return _fd;
		}

	private:

		std::streamsize
		do_fill() {
			// TODO 2016-03-09 this is not optimal solution,
			// but i don't know a better alternative
			std::streamsize old_egptr_offset = egptr() - eback();
			char_type* first = egptr();
			char_type* last = glast();
			std::streamsize n = 0;
			while ((n = _fd.read(first, last-first)) > 0) {
				first += n;
				setg(eback(), gptr(), first);
				if (first == last) {
					ggrow();
					first = egptr();
					last = glast();
				}
			}
			return first - (eback() + old_egptr_offset);
		}

	protected:

		char_type*
		gfirst() noexcept {
			return _gbuf.data();
		}

		char_type*
		glast() noexcept {
			return _gbuf.data() + _gbuf.size();
		}

		void
		rebase() noexcept {
			char_type* base = _gbuf.data();
			setg(
				base,
				base + (gptr()-eback()),
				base + (egptr()-eback())
			);
		}

		void
		ggrow() {
			_gbuf.resize(_gbuf.size() * 2);
			rebase();
		}

		void
		pgrow() {
			const pos_type off = pptr() - pbase();
			const pos_type n = epptr() - pbase();
			_pbuf.resize(_pbuf.size() * 2);
			char_type* base = _pbuf.data();
			setp(base, base + _pbuf.size());
			pbump(off);
		}

		signed_size_type
		psize() const {
			return static_cast<signed_size_type>(_pbuf.size());
		}

		signed_size_type
		gsize() const {
			return static_cast<signed_size_type>(_gbuf.size());
		}

		fd_type _fd;
		std::vector<char_type> _gbuf;
		std::vector<char_type> _pbuf;
	};

	template<class Ch, class Tr=std::char_traits<Ch>, class Fd=sysx::fildes>
	struct basic_ifdstream: public std::basic_istream<Ch> {
		typedef basic_fildesbuf<Ch,Tr,Fd> fildesbuf_type;
		typedef std::basic_istream<Ch,Tr> istream_type;
		explicit basic_ifdstream(Fd&& fd): istream_type(nullptr),
			_fildesbuf(std::move(fd), 512, 0) { this->init(&_fildesbuf); }
	private:
		fildesbuf_type _fildesbuf;
	};

	template<class Ch, class Tr=std::char_traits<Ch>, class Fd=sysx::fildes>
	struct basic_ofdstream: public std::basic_ostream<Ch> {
		typedef basic_fildesbuf<Ch,Tr,Fd> fildesbuf_type;
		typedef std::basic_ostream<Ch,Tr> ostream_type;
		explicit basic_ofdstream(Fd&& fd): ostream_type(nullptr),
			_fildesbuf(std::move(fd), 0, 512) { this->init(&_fildesbuf); }
	private:
		fildesbuf_type _fildesbuf;
	};

	template<class Ch, class Tr=std::char_traits<Ch>, class Fd=sysx::fildes>
	struct basic_fdstream: public std::basic_iostream<Ch> {
		typedef basic_fildesbuf<Ch,Tr,Fd> fildesbuf_type;
		typedef std::basic_iostream<Ch,Tr> iostream_type;
		explicit basic_fdstream(Fd&& fd): iostream_type(nullptr),
			_fildesbuf(std::move(fd), 512, 512) { this->init(&_fildesbuf); }
	private:
		fildesbuf_type _fildesbuf;
	};

	typedef basic_fildesbuf<char> fildesbuf;
	typedef basic_ifdstream<char> ifdstream;
	typedef basic_ofdstream<char> ofdstream;

}

namespace stdx {

	template<class Ch, class Tr, class Fd>
	struct type_traits<sysx::basic_fildesbuf<Ch,Tr,Fd>> {
		static constexpr const char*
		short_name() { return "fildesbuf"; }
		typedef sysx::buffer_category category;
	};

}

#endif // SYSX_FILDESBUF_HH
