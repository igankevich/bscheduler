namespace factory {

	typedef struct ::pollfd Basic_event;

	struct Event: public Basic_event {

		typedef decltype(Basic_event::events) Evs;
		typedef int fd_type;

		constexpr Event(): Basic_event{-1,0,0} {}
		constexpr explicit Event(int f): Basic_event{f,0,0} {}
		constexpr Event(Evs e, int f): Basic_event{f,e,0} {}

		constexpr Evs events() const { return this->Basic_event::revents; }
		void events(Evs rhs) { this->Basic_event::events = rhs; }

		void fd(fd_type rhs) { this->Basic_event::fd = rhs; }
		constexpr fd_type fd() const { return this->Basic_event::fd; }
		constexpr bool bad_fd() const { return this->fd() < 0; } 

		constexpr bool is_reading() const { return (this->events() & POLLIN) != 0; }
		constexpr bool is_writing() const { return (this->events() & POLLOUT) != 0; }
		constexpr bool is_closing() const {
			return (this->events() & (POLLHUP | POLLRDHUP)) != 0;
		}
		constexpr bool is_error() const {
			return (this->events() & (POLLERR | POLLNVAL)) != 0;
		}

		void no_reading() { this->Basic_event::revents &= ~POLLIN; }
		void writing() {
			this->Basic_event::events |= POLLOUT;
			this->Basic_event::revents |= POLLOUT;
		}
		void reading() {
			this->Basic_event::events |= POLLIN;
			this->Basic_event::revents |= POLLIN;
		}

		void setev(Evs rhs) { this->Basic_event::revents |= rhs; }
		ssize_t probe() const {
			char c;
			return ::recv(this->fd(), &c, 1, MSG_PEEK);
		}

		constexpr bool operator==(const Event& rhs) const { return this->fd() == rhs.fd(); }
		constexpr bool operator!=(const Event& rhs) const { return this->fd() != rhs.fd(); }
		constexpr bool operator< (const Event& rhs) const { return this->fd() <  rhs.fd(); }

		friend std::ostream& operator<<(std::ostream& out, const Event& rhs) {
			return out
				<< rhs.fd() << ' '
				<< (rhs.is_reading() ? 'r' : ' ')
				<< (rhs.is_writing() ? 'w' : ' ')
				<< (rhs.is_closing() ? 'c' : ' ')
				<< (rhs.is_error() ? 'e' : ' ');
		}

	};

	static_assert(sizeof(Event) == sizeof(Basic_event),
		"The size of Event does not match the size of Basic_event.");
	
	struct Poller {

		enum struct State: char {
			DEFAULT,
			STOPPING,
			STOPPED
		};

		Poller():
			_state(State::DEFAULT),
			_mgmt_pipe(),
			_events()
		{}
	
		int notification_pipe() const { return _mgmt_pipe.read_end(); }

		void notify() {
			char c = NOTIFY_SYMBOL;
			check("Poller::notify()", ::write(_mgmt_pipe.write_end(), &c, 1));
		}

		void notify_stopping() {
			char c = STOP_SYMBOL;
			check("Poller::notify_stopping()", ::write(_mgmt_pipe.write_end(), &c, 1));
		}

	
		template<class Callback>
		void run(Callback callback) {
			add(Event(POLLIN, _mgmt_pipe.read_end()));
			while (!stopped()) this->wait(callback);
		}

		bool stopped() const { return _state == State::STOPPED; }
		bool stopping() const { return _state == State::STOPPING; }

		void stop() {
			Logger<Level::COMPONENT>() << "Poller::stop()" << std::endl;
			::close(_mgmt_pipe.write_end());
		}
	
		void add(Event rhs) {
			Logger<Level::COMPONENT>() << "Poller::add(" << rhs.fd() << ", " << rhs << ")" << std::endl;
			_events.push_back(rhs);
		}

		Event* operator[](int fd) {
			auto pos = std::find(_events.begin(), _events.end(), Event(fd));
			return pos == _events.end() ? nullptr : &*pos;
		}

		void ignore(int fd) {
			auto pos = std::find(_events.begin(), _events.end(), Event(fd));
			if (pos != _events.end()) {
				Logger<Level::COMPONENT>() << "ignoring fd=" << pos->fd() << std::endl;
				pos->fd(-1);
			}
		}

	private:

		void consume_notify() {
			const size_t n = 20;
			char tmp[n];
			ssize_t c;
			while ((c = ::read(_mgmt_pipe.read_end(), tmp, n)) != -1) {
				if (std::any_of(tmp, tmp + c, [this] (char rhs) { return rhs == STOP_SYMBOL; })) {
					_state = State::STOPPING;
				}
			}
		}

		void erase(Event rhs) {
			auto pos = std::find(_events.begin(), _events.end(), rhs);
			if (pos != _events.end()) {
				_events.erase(pos);
			}
		}

		void erase(int fd) { erase(Event(fd)); }


		static int check_poll(const char* func, int ret) {
			return (errno == EINTR) ? ret : check(func, ret);
		}
	
		template<class Callback>
		void wait(Callback callback) {

			do {
				Logger<Level::COMPONENT>() << "poll()" << std::endl;
				check_poll("poll()", ::poll(_events.data(), _events.size(), -1));
			} while (errno == EINTR);

			size_t nfds = _events.size();
			for (size_t n=0; n<nfds; ++n) {
				Event e = _events[n];
				if (e.bad_fd()) {
					erase(e);
					--n;
					--nfds;
					continue;
				}
#if !HAVE_DECL_POLLRDHUP
				if (e.probe() == 0) {
					e.setev(POLLHUP);
				}
#endif
				if (e.events() == 0) continue;
				if (e.fd() == _mgmt_pipe.read_end()) {
					if (e.is_closing()) {
						Logger<Level::COMPONENT>() << "Stopping poller" << std::endl;
						_state = State::STOPPED;
					}
					if (e.is_reading()) {
						consume_notify();
						callback(e);
					}
				} else {
					callback(e);
					if (e.is_closing() || e.is_error()) {
						erase(e);
						--n;
						--nfds;
					}
				}
			}
		}

		friend std::ostream& operator<<(std::ostream& out, const Poller& rhs) {
			std::ostream_iterator<Event> it(out, ", ");
			std::copy(rhs._events.cbegin(), rhs._events.cend(), it);
			return out;
		}
	
		State _state;

		struct Pipe {

			Pipe() {
				check("pipe()", ::pipe(_fds));
				int flags = ::fcntl(read_end(), F_GETFL);
				::fcntl(read_end(), F_SETFL, flags | O_NONBLOCK | O_CLOEXEC);
			}

			~Pipe() {
				::close(_fds[0]);
				::close(_fds[1]);
			}

			int read_end() const { return _fds[0]; }
			int write_end() const { return _fds[1]; }

		private:
			int _fds[2];
		} _mgmt_pipe;
	
		std::vector<Event> _events;

		static const char STOP_SYMBOL   = 's';
		static const char NOTIFY_SYMBOL = '!';
	};

}
