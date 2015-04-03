#include "process.hh"

using namespace factory;

typedef Interval<Port> Port_range;

const Port DISCOVERY_PORT = 10000;

std::vector<Endpoint> all_peers;

struct Discovery: public Mobile<Discovery> {

	typedef uint64_t Time;
	typedef uint8_t State;
	typedef std::chrono::steady_clock Clock;

	Discovery() {}

	void act() {
		_state = 1;
		commit(remote_server());
	}

	void write_impl(Foreign_stream& out) {
		if (_state == 0) {
			_time = current_time();
		}
		out << _state << _time;
	}

	void read_impl(Foreign_stream& in) {
		in >> _state >> _time;
		if (_state == 1) {
			_time = current_time() - _time;
		}
	}

	Time time() const { return _time*0 + 123; }

	static Time current_time() {
		return Clock::now().time_since_epoch().count();
	}
	
	static void init_type(Type* t) {
		t->id(2);
	}

private:
	Time _time = 0;
	State _state = 0;
};

struct Peer {

	typedef Discovery::Time Time;
	typedef uint32_t Metric;

	Peer(): _addr() {}
	Peer(Endpoint a, Time t): _addr(a) { sample(t); }
	Peer(Endpoint a): _addr(a) {}
	Peer(const Peer& rhs):
		_t(rhs._t), _n(rhs._n), _addr(rhs._addr) {}

	Metric metric() const { return _t/1000/1000/1000; }

	void sample(Time rhs) {
		if (++_n == MAX_SAMPLES) {
			_n = 1;
			_t = 0;
		}
		_t = ((_n-1)*_t + rhs) / _n;
	}

	uint32_t num_samples() const { return _n; }
	Endpoint addr() const { return _addr; }

	bool operator<(const Peer& rhs) const {
		return metric() < rhs.metric()
			|| (metric() == rhs.metric() && _addr < rhs._addr);
	}

	Peer& operator=(const Peer& rhs) {
		_t = rhs._t;
		_n = rhs._n;
		_addr = rhs._addr;
		return *this;
	}

	friend std::ostream& operator<<(std::ostream& out, const Peer& rhs) {
		return out
			<< rhs._t
			<< ' ' << rhs._n
			<< ' ' << rhs._addr;
	}

	friend std::istream& operator>>(std::istream& in, Peer& rhs) {
		return in
			>> rhs._t
			>> rhs._n
			>> rhs._addr;
	}

private:
	Time _t = 0;
	uint32_t _n = 0;
	Endpoint _addr;

	static const uint32_t MAX_SAMPLES = 1000;
};

struct Vote: public Mobile<Vote> {

	enum Response : uint8_t {
		OK,
		RETRY,
		BAD_VOTE
	};

	Vote() {}
	explicit Vote(uint32_t l): _level(l) {}
	uint32_t level() const { return _level; }

	bool retry() const { return _state == RETRY; }
	bool bad_vote() const { return _state == BAD_VOTE; }
	bool ok() const { return _state == OK; }
	void response(Response rhs) { _state = rhs; }

	void write_impl(Foreign_stream& out) { out << _level << _state; }
	void read_impl(Foreign_stream& in) { in >> _level >> _state; }
	static void init_type(Type* t) {
		t->id(7);
	}
private:
	uint32_t _level = 0;
	uint8_t _state = OK;

};

struct Discoverer: public Identifiable<Kernel> {

	enum State {
		DISCONNECTED,
		CONNECTING,
		CONNECTED
	};

//	void act() {
//		auto peers = discover_peers();
//		_num_peers = std::accumulate(peers.cbegin(), peers.cend(), uint32_t(0),
//			[] (uint32_t sum, const Address_range& rhs)
//		{
//			return sum + rhs.end() - rhs.start();
//		});
//		std::for_each(peers.cbegin(), peers.cend(),
//			[this] (const Address_range& range)
//		{
//			typedef typename Address_range::Int I;
//			I st = range.start();
//			I en = range.end();
//			for (I a=st; a<en/* && a<st+10*/; ++a) {
//				Endpoint ep(a, port);
//				Logger(Level::DISCOVERY) << ep << std::endl;
//				Discovery_kernel* k = new Discovery_kernel;
//				k->parent(this);
//				remote_server()->send(k, ep);
//			}
//		});
//	}

	Discoverer(Endpoint endpoint):
		factory::Identifiable<Kernel>(endpoint.address()),
		_source(endpoint),
		_servers(create_servers(all_peers)),
		_downstream(),
		_upstream()
		{}

	void act() {
		_attempts++;
		_num_succeeded = 0;
		_num_failed = 0;
		_num_peers = 0;

		if (read_cache()) {
			Logger(Level::DISCOVERY)
				<< "reading cache " << std::endl;
			_state = CONNECTING;
			send_vote();
			return;
		}

		Logger(Level::DISCOVERY) << "attempt #" << _attempts << std::endl;

		std::vector<Discovery*> kernels;
		std::for_each(_servers.cbegin(), _servers.cend(),
			[this, &kernels] (const Endpoint& ep)
		{
			Peer* n = find_peer(ep);
			if (n == nullptr || n->num_samples() < MIN_SAMPLES) {
				Logger(Level::DISCOVERY) << "Sending to " << ep << std::endl;
				Discovery* k = new Discovery;
				k->parent(this);
				k->to(ep);
				kernels.push_back(k);
			}
		});

		// send discovery messages
		_num_peers = kernels.size();
		for (Discovery* d : kernels) {
			remote_server()->send(d, d->to());
		}

		// repeat when nothing is discovered
		if (_num_peers == 0) {
			throw "No peers to update";
		}
	}

	void react(Kernel* k) {
		if (k->result() == Result::ENDPOINT_NOT_CONNECTED &&
			k->type()->id() == 7)
		{
			Vote* vote = new Vote(_level);
			vote->to(_downstream.addr());
			vote->parent(this);
			vote->principal(_downstream.addr().address());
			remote_server()->send(vote);
		}
		if (k->moves_somewhere()) {
			Vote* vote = dynamic_cast<Vote*>(k);
			if (_state == DISCONNECTED) {
				Logger(Level::DISCOVERY) << "retry vote from "
					<< k->from() << ", level=" << vote->level() << std::endl;
				vote->result(Result::USER_ERROR);
				vote->principal(vote->parent());
				vote->response(Vote::RETRY);
				remote_server()->send(vote);
			} else if (vote->from() == _downstream.addr() && vote->from() < _source) {
				Logger(Level::DISCOVERY) << "bad vote from "
					<< k->from() << ", level=" << vote->level() << std::endl;
				vote->result(Result::USER_ERROR);
				vote->principal(vote->parent());
				vote->response(Vote::BAD_VOTE);
				remote_server()->send(vote);
			} else {
				_upstream.push_back(_peers[vote->from()]);
				_state = CONNECTED;
				Logger(Level::DISCOVERY) << "good vote from "
					<< k->from() << ", level=" << vote->level() << std::endl;
				vote->response(Vote::OK);
				vote->commit(remote_server());
				if (_upstream.size() == all_peers.size() - 1) {
					Logger log(Level::DISCOVERY);
					log << "Hail the king! His boys: ";
					std::ostream_iterator<Peer> it(log.ostream(), ", ");
					std::copy(_upstream.cbegin(), _upstream.cend(), it);
					log << std::endl;
//					if (_source != all_peers[0]) {
//						throw std::runtime_error("The first endpoint has not become a king.");
//					}
					__factory.stop();
				}
			}
		} else {
			if (k->type()->id() == 7) {
				if (k->result() == Result::USER_ERROR && _sorted_peers.size() >= 2) {
					Vote* vote = dynamic_cast<Vote*>(k);
					if (vote->bad_vote()) {
						_downstream = Peer();
					} else {
						_downstream = vote->retry() ? *_sorted_peers.cbegin() : *(++_sorted_peers.cbegin());
						Vote* vote = new Vote(_level);
						vote->to(_downstream.addr());
						vote->parent(this);
						vote->principal(_downstream.addr().address());
						remote_server()->send(vote);
						Logger(Level::DISCOVERY) << "voting again for " << _downstream.addr() << std::endl;
					}
				}
				return;
			}
			Discovery* d = dynamic_cast<Discovery*>(k);
			if (d->result() == Result::SUCCESS) {
				_num_succeeded++;
				if (Peer* p = find_peer(d->from())) {
					p->sample(d->time());
				} else {
					_peers[d->from()] = Peer(d->from(), d->time());
				}
			} else {
				_num_failed++;
//				auto result = _peers.find(d->from());
//				if (result != _peers.end()) {
//					Peer* n = result->second;
//					_peers.erase(result->first);
//					_sorted_peers.erase(n);
//					delete n;
//				}
			}

			Logger(Level::DISCOVERY) << "result #"
				<< _attempts << ' '
				<< num_succeeded() << '+'
				<< num_failed() << '/'
				<< num_peers() << ' '
				<< min_samples() << std::endl;
			
			if (_num_succeeded + _num_failed == _num_peers) {
				
				if (min_samples() == MIN_SAMPLES) {

					sort_peers(_peers, _sorted_peers);
					_state = CONNECTING;

					Logger log(Level::DISCOVERY);
					log << "Peers: ";
					std::ostream_iterator<Peer> it(log.ostream(), ", ");
					std::copy(_sorted_peers.cbegin(), _sorted_peers.cend(), it);
					log << std::endl;

					send_vote();
				} else {
					act();
				}
			}
		}
	}

	uint32_t num_failed() const { return _num_failed; }
	uint32_t num_succeeded() const { return _num_succeeded; }
	uint32_t num_peers() const { return _num_peers; }

private:

	static void sort_peers(const std::map<Endpoint,Peer>& peers, std::set<Peer>& y) {
		y.clear();
		std::transform(peers.begin(), peers.end(), std::inserter(y, y.begin()), get_peer);
	}

	static Peer get_peer(const std::pair<Endpoint,Peer>& rhs) {
		return std::get<1>(rhs);
	}
		
	void send_vote() {
		if (!_sorted_peers.empty()) {
			_downstream = *_sorted_peers.cbegin();
			Vote* vote = new Vote(_level);
			vote->to(_downstream.addr());
			vote->parent(this);
			vote->principal(_downstream.addr().address());
			remote_server()->send(vote);
			Logger(Level::DISCOVERY) << "voting for " << _downstream.addr() << std::endl;
		}
	}

	std::string cache_filename() const {
		std::stringstream s;
		s << "/tmp/";
		s << _source << ".cache";
		return s.str();
	}

	bool read_cache() {
		std::ifstream in(cache_filename());
		bool success = in.is_open();
		if (success) {
			while (!in.eof()) {
				Peer p;
				in >> p >> std::ws;
				_peers[p.addr()] = p;
			}
			if (_downstream.addr() != Endpoint()) {
				_peers[_downstream.addr()] = _downstream;
			}
			sort_peers(_peers, _sorted_peers);
		}
		return success;
	}

	void write_cache() {
		std::ofstream  out(cache_filename());
		std::ostream_iterator<Peer> it(out, "\n");
		std::copy(_upstream.cbegin(), _upstream.cend(), it);
	}

	std::vector<Endpoint> create_servers(std::vector<Endpoint> servers) {
		std::vector<Endpoint> tmp;
		std::copy_if(servers.cbegin(), servers.cend(),
			std::back_inserter(tmp), [this] (Endpoint addr) {
				return addr != this->_source;
			});
		return tmp;
	}

	uint32_t min_samples() const {
		return std::accumulate(_peers.cbegin(), _peers.cend(),
			std::numeric_limits<uint32_t>::max(),
			[] (uint32_t m, const std::pair<Endpoint,Peer>& rhs)
		{
			return std::min(m, rhs.second.num_samples());
		});
	}

	Peer* find_peer(Endpoint addr) {
		auto result = _peers.find(addr);
		return result == _peers.end() ? nullptr : &result->second;
	}

	Endpoint _source;

	std::map<Endpoint, Peer> _peers;
	std::set<Peer> _sorted_peers;
	std::vector<Endpoint> _servers;
	Peer _downstream;
	std::vector<Peer> _upstream;
	State _state = DISCONNECTED;

	uint32_t _num_peers = 0;
	uint32_t _num_succeeded = 0;
	uint32_t _num_failed = 0;

	uint32_t _attempts = 0;
	uint32_t _level = 0;

	static const uint32_t MAX_ATTEMPTS = 17;
	static const uint32_t MIN_SAMPLES = 7;
};

int num_peers() {
	std::stringstream s;
	s << ::getenv("NUM_PEERS");
	int n = 0;
	s >> n;
	if (n <= 1) {
		n = 3;
	}
	return n;
}

bool write_cache() { return ::getenv("WRITE_CACHE") != NULL; }

void generate_all_peers(uint32_t npeers, std::string base_ip) {
	all_peers.clear();
	uint32_t start = Endpoint(base_ip.c_str(), 0).address();
	uint32_t end = start + npeers;
	for (uint32_t i=start; i<end; ++i) {
		Endpoint endpoint(i, DISCOVERY_PORT);
		all_peers.push_back(endpoint);
	}
}

std::string cache_filename(Endpoint source) {
	std::stringstream s;
	s << "/tmp/";
	s << source << ".cache";
	return s.str();
}

void write_cache_all() {
	int npeers = all_peers.size();
	for (int i=1; i<npeers; ++i) {
		Endpoint endpoint = all_peers[i];
		std::ofstream out(cache_filename(endpoint));
		out << Peer(all_peers[0]) << '\n';
	}
	{
		Endpoint master = all_peers[0];
		std::ofstream out(cache_filename(master));
		out << Peer() << '\n';
		std::ostream_iterator<Peer> it(out, "\n");
		std::copy(all_peers.cbegin()+1, all_peers.cend(), it);
	}
}


struct App {
	int run(int argc, char* argv[]) {
		int retval = 0;
		if (argc <= 2) {
			try {
				int npeers = num_peers();
				std::string base_ip = argc == 2 ? argv[1] : "127.0.0.1"; 
				generate_all_peers(npeers, base_ip);
				if (write_cache()) {
					write_cache_all();
					return 0;
				}

				Process_group processes;
				int start_id = 1000;
				for (Endpoint endpoint : all_peers) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					processes.add([endpoint, &argv, start_id, npeers, &base_ip] () {
						Process::env("START_ID", start_id);
						return Process::execute(argv[0],
							"--bind-addr", endpoint,
							"--num-peers", npeers,
							"--base-ip", base_ip);
					});
					start_id += 1000;
				}

				Logger(Level::DISCOVERY) << "Forked " << processes << std::endl;
				
				retval = processes.wait();

			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		} else {
			int npeers = 3;
			Endpoint endpoint(get_bind_address(), DISCOVERY_PORT);
			std::string base_ip = "127.0.0.1";
			Command_line cmdline(argc, argv);
			cmdline.parse([&endpoint, &npeers, &base_ip](const std::string& arg, std::istream& in) {
				     if (arg == "--bind-addr") { in >> endpoint; }
				else if (arg == "--num-peers") { in >> npeers; }
				else if (arg == "--base-ip")   { in >> base_ip; }
			});
			Logger(Level::DISCOVERY) << "Bind address " << endpoint << std::endl;
			generate_all_peers(npeers, base_ip);
			try {
				the_server()->add(0);
				the_server()->start();
				remote_server()->socket(endpoint);
				remote_server()->start();
				the_server()->send(new Discoverer(endpoint));
				__factory.wait();
			} catch (std::exception& e) {
				std::cerr << e.what() << std::endl;
				retval = 1;
			}
		}
		return retval;
	}
};