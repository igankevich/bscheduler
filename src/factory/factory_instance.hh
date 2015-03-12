#include <signal.h>
#include <strings.h>
#include <fenv.h>

namespace factory {

	void emergency_shutdown(int signum);
	
	struct Basic_factory {

		Basic_factory():
			_local_server(),
			_remote_server(),
			_discovery_server(),
			_repository()
		{
			init_signal_handlers();
		}

		virtual ~Basic_factory() {}

		void start() {
			_local_server.start();
			_remote_server.start();
			_discovery_server.start();
		}

		void stop() {
			_local_server.stop();
			_remote_server.stop();
			_discovery_server.stop();
		}

		void wait() {
			_local_server.wait();
			_remote_server.wait();
			_discovery_server.wait();
		}

		Local_server* local_server() { return &_local_server; }
		Remote_server* remote_server() { return &_remote_server; }
		Discovery_server* discovery_server() { return &_discovery_server; }
		Repository_stack* repository() { return &_repository; }

	private:

		void init_signal_handlers() {
			struct ::sigaction action;
			::bzero(&action, sizeof(struct ::sigaction));
			action.sa_handler = emergency_shutdown;
			::sigaction(SIGTERM, &action, NULL);
			::sigaction(SIGINT, &action, NULL);
			::sigaction(SIGSEGV, &action, NULL);
			::feenableexcept(FE_INVALID | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW);
			::sigaction(SIGFPE, &action, NULL);
		}

		Local_server _local_server;
		Remote_server _remote_server;
		Discovery_server _discovery_server;
		Repository_stack _repository;
	};


}

