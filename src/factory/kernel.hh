namespace factory {

	namespace components {

		struct Basic_kernel {

			Basic_kernel(): _result(Result::UNDEFINED) {}
			virtual ~Basic_kernel() {}

			virtual void act() {}

			Result result() const { return _result; }
			void result(Result rhs) { _result = rhs; }

		private:
			Result _result;
		};

		template<class K>
		struct Kernel_ref {

			Kernel_ref(): _kernel(nullptr), _temp(false) {}
			Kernel_ref(K* rhs): _kernel(rhs), _temp(false) {}
			Kernel_ref(Id rhs): _kernel(new Transient_kernel(rhs)), _temp(true) {}
			Kernel_ref(const Kernel_ref& rhs): _kernel(rhs._kernel), _temp(rhs._temp) {
				if (_temp) {
					Id i = _kernel->id();
					_kernel = new Transient_kernel(i);
				}
			}
			~Kernel_ref() {
				if (_temp) delete _kernel;
			}

			// dereference operators
			K* operator->() { return _kernel; }
			const K* operator->() const { return _kernel; }
			K& operator*() { return *_kernel; }
			const K& operator*() const { return *_kernel; }

			Kernel_ref& operator=(const Kernel_ref& rhs) {
				if (_temp) {
					delete _kernel;
				}
				_temp = rhs._temp;
				if (_temp) {
					Id i = rhs._kernel->id();
					_kernel = new Transient_kernel(i);
				} else {
					_kernel = rhs._kernel;
				}
				return *this;
			}

			Kernel_ref& operator=(K* rhs) {
				if (_temp) {
					delete _kernel;
					_temp = false;
				}
				_kernel = rhs;
				return *this;
			}

			Kernel_ref& operator=(Id rhs) {
				if (_temp) {
					delete _kernel;
				} else {
					_temp = true;
				}
				_kernel = new Transient_kernel(rhs);
				return *this;
			}

			K* ptr() { return _kernel; }
			const K* ptr() const { return _kernel; }

			explicit operator bool() const { return _kernel != nullptr; }
			bool operator !() const { return _kernel == nullptr; }

			friend std::ostream& operator<<(std::ostream& out, const Kernel_ref<K>& rhs) {
				return out << rhs->id();
			}

		private:
			K* _kernel;
			bool _temp;

			// No one lives forever.
			struct Transient_kernel: public Identifiable<K, Type<K>> {
				explicit Transient_kernel(Id i): Identifiable<K, Type<K>>(i, false) {}
			};
		};


		// The class ensures that certain methods (e.g. read, write)
		// are called in a sequence starting from method in the top superclass
		// and ending with method in the bottom subclass (like constructors and destructors).
		// In contrast to simply overriding method
		// this approach takes care of all superclasses' members being properly marshalled and demarshalled
		// from the data stream thus ensuring that no method's top implementations are omitted.
		template<class Sub, class Super>
		struct Kernel_link: public Super {

			void read(Foreign_stream& in) {
				Super::read(in);
				static_cast<Sub*>(this)->Sub::read_impl(in);
			}

			void write(Foreign_stream& out) {
				Super::write(out);
				static_cast<Sub*>(this)->Sub::write_impl(out);
			}

		};

		template<class A>
		struct Principal: public Kernel_link<Principal<A>, A> {

			typedef Principal<A> This;
			typedef Kernel_ref<This> Ref;

			Principal(): _parent(nullptr), _principal(nullptr) {}

			const Ref& principal() const { return _principal; }
			Ref principal() { return _principal; }
			void principal(Ref rhs) { _principal = rhs; }
			void principal(This* rhs) { _principal = rhs; }

			size_t hash() const {
				return _principal && _principal->identifiable()
					? _principal->id()
					: size_t(_principal.ptr()) / alignof(size_t);
			}

			const Ref& parent() const { return _parent; }
			Ref parent() { return _parent; }
			void parent(Ref p) { _parent = p; }
			void parent(This* p) { _parent = p; }

			bool moves_upstream() const { return this->result() == Result::UNDEFINED && !_principal && _parent; }
			bool moves_downstream() const { return this->result() != Result::UNDEFINED && _principal && _parent; }
			bool moves_somewhere() const { return this->result() == Result::UNDEFINED && _principal && _parent; }
			bool moves_everywhere() const { return !_principal && !_parent; }

			void read_impl(Foreign_stream& in) {
				if (_parent) {
					std::stringstream s;
					s << "Parent is not null while reading from the data stream. Parent=";
					s << _parent;
					throw Error(s.str(), __FILE__, __LINE__, __func__);
				}
				Id parent_id;
				in >> parent_id;
				Logger(Level::KERNEL) << "READING PARENT " << parent_id << std::endl;
				if (parent_id != ROOT_ID) {
					_parent = parent_id;
				}
				if (_principal.ptr() != nullptr) {
					throw Error("Principal kernel is not null while reading from the data stream.",
						__FILE__, __LINE__, __func__);
				}
				Id principal_id;
				in >> principal_id;
				Logger(Level::KERNEL) << "READING PRINCIPAL " << principal_id << std::endl;
				_principal = principal_id;
			}

			void write_impl(Foreign_stream& out) {
				out << (!_parent ? ROOT_ID : _parent->id());
				out << (!_principal ? ROOT_ID : _principal->id());
			}

			virtual void react(This*) {
				std::stringstream msg;
				msg << "Empty react in ";
				if (const Type<This>* tp = type()) {
					msg << *tp;
				} else {
					msg << "unknown type";
				}
				throw Error(msg.str(), __FILE__, __LINE__, __func__);
			}

			virtual void error(This* rhs) { react(rhs); }

			virtual const Type<This>* type() const { return nullptr; }

			void run_act() {
				switch (this->result()) {
					case Result::UNDEFINED:
						this->act();
//						this->result(Result::SUCCESS);
						break;
					default:
						Logger(Level::KERNEL) << "Result is defined" << std::endl;
						if (!_principal) {
							Logger(Level::KERNEL) << "Principal is null" << std::endl;
							if (!_parent) {
								delete this;
								Logger(Level::KERNEL) << "SHUTDOWN" << std::endl;
								factory_stop();
							}
						} else {
							Logger(Level::KERNEL) << "Principal is not null" << std::endl;
							bool del = *_principal == *_parent;
							if (this->result() == Result::SUCCESS) {
								Logger(Level::KERNEL) << "Principal react" << std::endl;
								_principal->react(this);
							} else {
								Logger(Level::KERNEL) << "Principal error" << std::endl;
								_principal->error(this);
							}
							if (del) {
								Logger(Level::KERNEL) << "Deleting kernel" << std::endl;
								delete this;
							}
						}
				}
			}

			friend std::ostream& operator<<(std::ostream& out, const This& rhs) {
				return out << '('
					<< (rhs.moves_upstream()   ? 'U' : ' ')
					<< (rhs.moves_downstream() ? 'D' : ' ') 
					<< (rhs.moves_somewhere()  ? 'S' : ' ') 
					<< (rhs.moves_everywhere()  ? 'B' : ' ') 
					<< ','
					<< rhs.id() << ','
					<< rhs.from() << ','
					<< rhs.to() << ','
					<< rhs.result()
					<< ')';
			}
		
		public:
			template<class S>
			inline void upstream(S* this_server, This* a) {
				a->parent(this);
				this_server->send(a);
			}

			template<class S>
			inline void downstream(S* this_server, This* a) {
				a->parent(this);
				a->principal(this);
				this_server->send(a);
			}

			template<class S>
			inline void downstream(S* this_server, This* a, This* b) {
				a->principal(b);
				this->result(Result::SUCCESS);
				this_server->send(a);
			}

			template<class S>
			inline void commit(S* this_server) {
				downstream(this_server, this, _parent.ptr());
			}

		private:
			Kernel_ref<This> _parent;
			Kernel_ref<This> _principal;
		};

		template<class A>
		struct Service: public A {
			virtual void stop() = 0;
//			virtual void wait() = 0;
		};


		template<class K>
		struct Mobile: public K {

			virtual void read(Foreign_stream& in) { 
				static_assert(sizeof(uint16_t)== sizeof(Result), "Result has bad type.");
				uint16_t r;
				in >> r;
				this->result(Result(r));
				Logger(Level::KERNEL) << "Reading result = " << r << std::endl;
				in >> _id;
			}

			virtual void write(Foreign_stream& out) {
				static_assert(sizeof(uint16_t)== sizeof(Result), "Result has bad type.");
				uint16_t r = static_cast<uint16_t>(this->result());
				Logger(Level::KERNEL) << "Writing result = " << r << std::endl;
				out << r;
				Logger(Level::KERNEL) << "Writing id = " << _id << std::endl;
				out << _id;
			}

			virtual void read_impl(Foreign_stream&) {}
			virtual void write_impl(Foreign_stream&) {}

			Id id() const { return _id; }
			void id(Id rhs) { _id = rhs; }
			bool identifiable() const { return _id != ROOT_ID; }

			virtual Endpoint from() const { return _src; }
			virtual void from(Endpoint rhs) { _src = rhs; }

			virtual Endpoint to() const { return _dst; }
			virtual void to(Endpoint rhs) { _dst = rhs; }

			bool operator==(const Mobile<K>& rhs) const {
				return this == &rhs || (id() != ROOT_ID && rhs.id() != ROOT_ID && id() == rhs.id());
			}

		private:
			Id _id = ROOT_ID;
			Endpoint _src;
			Endpoint _dst;
		};

	}

}
