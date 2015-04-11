#ifdef FACTORY_FOREIGN_STREAM
namespace factory {
	// TODO: this is not portable
	Foreign_stream& operator<<(Foreign_stream& out, const Endpoint& rhs) {
		return out << rhs.addr()->sin_addr.s_addr << rhs.addr()->sin_port;
	}
	Foreign_stream& operator>>(Foreign_stream& in, Endpoint& rhs) {
		rhs.addr()->sin_family = AF_INET;
		return in >> rhs.addr()->sin_addr.s_addr >> rhs.addr()->sin_port;
	}
}
#else
namespace factory {

	std::istream& getline_no_white_space(std::istream& in, std::string& str, char delim) {
		char ch;
		while ((ch = in.get()) != std::char_traits<char>::eof()
			&& !std::isspace(ch) && ch != delim)
		{
			str.push_back(ch);
		}
		if (ch != delim) {
			in.putback(ch);
		}
		return in;
	}

	typedef std::string Host;
	typedef uint16_t Port;

	struct Endpoint {

		Endpoint() { std::memset((void*)&_addr, 0, sizeof(_addr)); }
		Endpoint(const Host& h, Port p) { addr(h.c_str(), p); }
		Endpoint(uint32_t h, Port p) { addr(h, p); }
		Endpoint(const Endpoint& rhs) { addr(&rhs._addr); }
		Endpoint(struct ::sockaddr_in* rhs) { addr(rhs); }

		Endpoint& operator=(const Endpoint& rhs) {
			addr(&rhs._addr);
			return *this;
		}

		bool operator<(const Endpoint& rhs) const {
			return _addr.sin_addr.s_addr < rhs._addr.sin_addr.s_addr
				|| (_addr.sin_addr.s_addr == rhs._addr.sin_addr.s_addr
				&& _addr.sin_port < rhs._addr.sin_port);
		}

		bool operator==(const Endpoint& rhs) const {
			return _addr.sin_addr.s_addr == rhs._addr.sin_addr.s_addr
				&& _addr.sin_port == rhs._addr.sin_port;
		}

		bool operator!=(const Endpoint& rhs) const {
			return _addr.sin_addr.s_addr != rhs._addr.sin_addr.s_addr
				|| _addr.sin_port != rhs._addr.sin_port;
		}

		friend std::ostream& operator<<(std::ostream& out, const Endpoint& rhs) {
			char host[64];
			check_inet("inet_ntop()", ::inet_ntop(AF_INET, &rhs._addr.sin_addr.s_addr, host, sizeof(host)));
			return out << host << ':' << ntohs(rhs._addr.sin_port);
		}

		friend std::istream& operator>>(std::istream& in, Endpoint& rhs) {
			std::string host;
			Port port;
			in >> std::ws;
			getline_no_white_space(in, host, ':');
			std::cout << "Host = '" << host << "'." << std::endl;
			std::ios_base::fmtflags oldf = in.flags() & std::ios::skipws;
			in >> std::noskipws >> port;
			in.flags(oldf);
			if (in) {
				try {
					rhs.addr(host.c_str(), port);
				} catch (...) {
					in.setstate(std::ios_base::failbit);
				}
			}
			return in;
		}

		Host host() const {
			char h[64];
			check_inet("inet_ntop()",
				::inet_ntop(AF_INET, &_addr.sin_addr.s_addr, h, sizeof(h)));
			return Host(h);
		}

		uint32_t address() const { return ntohl(_addr.sin_addr.s_addr); }
		Port port() const { return ntohs(_addr.sin_port); }
		void port(Port rhs) { _addr.sin_port = htons(rhs); }

		uint32_t position(uint32_t netmask) const {
			uint32_t a = address();
			return a - (a & netmask);
		}

		struct ::sockaddr_in* addr() { return &_addr; }
		const struct ::sockaddr_in* addr() const { return &_addr; }

		explicit operator bool() const { return _addr.sin_addr.s_addr != 0; }
		bool operator !() const { return _addr.sin_addr.s_addr == 0; }

	private:
	
		void addr(const struct ::sockaddr_in* rhs) {
			std::memcpy((void*)&_addr, (const void*)rhs, sizeof(_addr));
		}

		void addr(const char* h, Port p) {
			_addr.sin_family = AF_INET;
			_addr.sin_port = htons(p);
			int ret = ::inet_pton(AF_INET, h, &_addr.sin_addr);
			if (ret == 0 || ret == -1) {
				throw ret;
//				std::stringstream msg;
//				msg << "inet_pton(). Bad address: '" << h << "'.";
//				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}
		}

		void addr(const uint32_t h, Port p) {
			struct ::sockaddr_in a;
			a.sin_family = AF_INET;
			a.sin_addr.s_addr = htonl(h);
			a.sin_port = htons(p);
			addr(&a);
		}

		struct ::sockaddr_in _addr;
	};

}
#endif
