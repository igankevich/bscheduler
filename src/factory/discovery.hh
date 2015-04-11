namespace factory {

	template<class I>
	I log2(I x) {
		I n = 0;
		while (x >>= 1) n++;
		return n;
	}

	uint32_t log(uint32_t x, uint32_t p = 1) {
		uint32_t n = 0;
		while (x >>= p) n++;
		return n;
	}

	template<class I>
	struct Interval {

		typedef I Int;
	
		Interval(): _start(0), _end(0) {}
		Interval(I a, I b): _start(a), _end(b) {}
		Interval(const Interval<I>& rhs): _start(rhs._start), _end(rhs._end) {}
	
		I start() const { return _start; }
		I end() const { return _end; }
	
		bool overlaps(const Interval<I>& rhs) const {
			return (_start < rhs._start && _end > rhs._start)
				|| (rhs._start < _start && rhs._end > _start);
		}
	
		Interval<I>& operator+=(const Interval<I>& rhs) {
			_start = std::min(_start, rhs._start);
			_end = std::max(_end, rhs._end);
			return *this;
		}
		
		bool operator<(const Interval<I>& rhs) const {
			return _start < rhs._start;
		}
	
		friend std::ostream& operator<<(std::ostream& out, const Interval<I>& rhs) {
			return out << rhs._start << ' ' << rhs._end;
		}
	
		friend std::istream& operator>>(std::istream& in, Interval<I>& rhs) {
			return in >> rhs._start >> rhs._end;
		}
	
		bool empty() const { return _start >= _end; }
		I count() const { return _end - _start; }
	
	private:
		I _start, _end;
	};
	
	typedef Interval<uint32_t> Address_range;
	
	union Address {
	
		explicit Address(uint32_t a=0): _addr(a) {}
	
		friend std::ostream& operator<<(std::ostream& out, const Address& rhs) {
			return out
				<< int(rhs._bytes[3]) << '.'
				<< int(rhs._bytes[2]) << '.'
				<< int(rhs._bytes[1]) << '.'
				<< int(rhs._bytes[0]);
		}
	
	private:
		uint32_t _addr;
		unsigned char _bytes[sizeof(_addr)];
	};
	
	std::vector<Address_range> discover_neighbours() {
	
		struct ::ifaddrs* ifaddr;
		check("getifaddrs()", ::getifaddrs(&ifaddr));
	
		std::set<Address_range> ranges;
	
		for (struct ::ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

			if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
				// ignore non-internet networks
				continue;
			}

			Endpoint addr(ifa->ifa_addr);
			if (addr.address() == Endpoint("127.0.0.1", 0).address()) {
				// ignore localhost and non-IPv4 addresses
				continue;
			}

			Endpoint netmask(ifa->ifa_netmask);
			if (netmask.address() == Endpoint("255.255.255.255",0).address()) {
				// ignore wide-area networks
				continue;
			}
	
			uint32_t addr_long = addr.address();
			uint32_t mask_long = netmask.address();
	
			uint32_t start = (addr_long & mask_long) + 1;
			uint32_t end = (addr_long & mask_long) + (~mask_long);
	
			ranges.insert(Address_range(start, addr_long));
			ranges.insert(Address_range(addr_long+1, end));
		}
	
		// combine overlaping ranges
		std::vector<Address_range> sorted_ranges;
		Address_range prev_range;
		std::for_each(ranges.cbegin(), ranges.cend(),
			[&sorted_ranges, &prev_range](const Address_range& range)
		{
			if (prev_range.empty()) {
				prev_range = range;
			} else {
				if (prev_range.overlaps(range)) {
					prev_range += range;
				} else {
					sorted_ranges.push_back(prev_range);
					prev_range = range;
				}
			}
		});
	
		if (!prev_range.empty()) {
			sorted_ranges.push_back(prev_range);
		}
	
		std::for_each(sorted_ranges.cbegin(), sorted_ranges.cend(),
			[] (const Address_range& range)
		{
			std::clog << Address(range.start()) << '-' << Address(range.end()) << '\n';
		});
	
		::freeifaddrs(ifaddr);
	
		return sorted_ranges;
	}

	Endpoint random_endpoint(const std::vector<Address_range>& addrs, Port port) {

		typedef typename Address_range::Int I;

		size_t n = addrs.size();
		I total_count = 0;
		for (size_t i=0; i<n; ++i) {
			total_count += addrs[i].end() - addrs[i].start();
		}

		typedef std::default_random_engine::result_type Res_type;
		static std::default_random_engine generator(static_cast<Res_type>(std::chrono::steady_clock::now().time_since_epoch().count()));
		std::uniform_int_distribution<I> distribution(0, total_count-1);
		I m = distribution(generator);

		size_t i = 0;
		I cnt;
		while (m > (cnt = addrs[i].end() - addrs[i].start()) && i < n) {
			m -= cnt;
			i++;
		}

		I addr = addrs[i].start() + m;
		return Endpoint(addr, port);
	}

	uint32_t get_bind_address() {

		uint32_t ret = 0;
	
		struct ::ifaddrs* ifaddr;
		check("getifaddrs()", ::getifaddrs(&ifaddr));
	
		for (struct ::ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

			if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
				// ignore non-internet networks
				continue;
			}

			Endpoint addr(ifa->ifa_addr);
			if (addr.address() == Endpoint("127.0.0.1", 0).address()) {
				// ignore localhost and non-IPv4 addresses
				continue;
			}

			Endpoint netmask(ifa->ifa_netmask);
			if (netmask.address() == Endpoint("255.255.255.255",0).address()) {
				// ignore wide-area networks
				continue;
			}
	
			ret = addr.address();
	
			break;
		}
	
		::freeifaddrs(ifaddr);
	
		return ret;
	}

}
