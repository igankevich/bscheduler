#include <factory/base/error_handler.hh>
#include <factory/ppl/application_kernel.hh>
#include <factory/api.hh>
#include <iostream>
#include <string>
#include <vector>

#include "factory_socket.hh"

using asc::Application_kernel;
using namespace asc;

Application_kernel*
new_application_kernel(int argc, char* argv[]) {
	typedef Application_kernel::container_type container_type;
	container_type args, env;
	for (int i=1; i<argc; ++i) {
		args.emplace_back(argv[i]);
	}
	for (char** first=environ; *first; ++first) {
		env.emplace_back(*first);
	}
	return new Application_kernel(args, env);
}

class Main: public kernel {

private:
	int _argc;
	char** _argv;

public:
	Main(int argc, char** argv):
	_argc(argc),
	_argv(argv)
	{}

	void
	act() override {
		upstream<Remote>(
			this,
			new_application_kernel(this->_argc, this->_argv)
		);
	}

	void
	react(kernel* child) {
		Application_kernel* app = dynamic_cast<Application_kernel*>(child);
		if (app->return_code() != asc::exit_code::success) {
			std::cerr << app->error() << std::endl;
		}
		std::clog << "finish" << std::endl;
		commit<Local>(this, app->return_code());
	}

};

int main(int argc, char* argv[]) {
	if (argc <= 1) {
		std::cerr << "Please, specify at least one argument." << std::endl;
		return 1;
	}
	asc::install_error_handler();
	asc::types.register_type<Application_kernel>();
	factory_guard g;
	asc::factory.parent().use_localhost(false);
	asc::factory.parent()
		.add_client(sys::endpoint(FACTORY_UNIX_DOMAIN_SOCKET));
	send<Local>(new Main(argc, argv));
	return asc::wait_and_return();
}
