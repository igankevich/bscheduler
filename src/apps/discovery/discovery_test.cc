#include <map>

#include <sysx/socket.hh>
#include <sysx/cmdline.hh>
#include <sysx/network.hh>

#include <factory/factory.hh>
#include <factory/server/cpu_server.hh>
#include <factory/server/timer_server.hh>
#include <factory/server/nic_server.hh>

#include "distance.hh"
#include "cache_guard.hh"
#include "hierarchy.hh"
#include "springy_graph_generator.hh"
#include "hierarchy_with_graph.hh"
#include "test.hh"

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sysx::buffer_category>:
	public std::true_type {};

	template<>
	struct disable_log_category<factory::components::kernel_category>:
	public std::true_type {};

	template<>
	struct disable_log_category<factory::components::server_category>:
	public std::true_type {};

}

namespace factory {
	inline namespace this_config {

		struct config {
			typedef components::Managed_object<components::Server<config>> server;
			typedef components::Principal<config> kernel;
			typedef components::CPU_server<config> local_server;
			typedef components::NIC_server<config, sysx::socket> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::No_server<config> app_server;
			typedef components::No_server<config> principal_server;
			typedef components::No_server<config> external_server;
			typedef components::Basic_factory<config> factory;
		};

		typedef config::kernel Kernel;
		typedef config::server Server;
	}
}

using namespace factory;
using namespace factory::this_config;

#include "apps/autoreg/autoreg_app.hh"
#include "ping_pong.hh"
#include "delayed_shutdown.hh"
#include "master_discoverer.hh"

constexpr const char* role_master = "master";
constexpr const char* role_slave = "slave";

template<class Address>
struct Main: public Kernel {

	typedef Address addr_type;
	typedef typename sysx::ipaddr_traits<addr_type> traits_type;
	typedef Negotiator<addr_type> negotiator_type;
	typedef discovery::Hierarchy_with_graph<discovery::Hierarchy<addr_type>> hierarchy_type;
	typedef discovery::Distance_in_tree<addr_type> distance_type;
	typedef Master_discoverer<addr_type, hierarchy_type, distance_type> discoverer_type;
	typedef stdx::log<Main> this_log;

	Main(Server& this_server, int argc, char* argv[]):
	_cmdline(argc, argv, {
		sysx::cmd::ignore_first_arg(),
		sysx::cmd::make_option({"--role"}, _role),
		sysx::cmd::make_option({"--network"}, _network),
		sysx::cmd::make_option({"--port"}, _port),
		sysx::cmd::make_option({"--timeout"}, _timeout),
		sysx::cmd::make_option({"--normal"}, _normal),
		sysx::cmd::make_option({"--master"}, _masteraddr)
	})
	{}

	void
	act(Server& this_server) {
		parse_cmdline_args(this_server);
		this_server.factory()->types().register_type(negotiator_type::static_type());
		this_server.factory()->types().register_type(Ping::static_type());
		this_server.factory()->types().register_type(autoreg::Generator1<float,autoreg::Uniform_grid>::static_type());
		this_server.factory()->types().register_type(autoreg::Wave_surface_generator<float,autoreg::Uniform_grid>::static_type());
		this_server.factory()->types().register_type(autoreg::Autoreg_model<float>::static_type());
		this_server.factory()->types().register_type(Secret_agent::static_type());
		if (this_server.factory()->exit_code()) {
			commit(this_server.local_server());
		} else {
			const sysx::ipv4_addr netmask = sysx::ipaddr_traits<sysx::ipv4_addr>::loopback_mask();
			const sysx::endpoint bind_addr(_network.address(), _port);
			this_server.remote_server()->bind(bind_addr, netmask);
			const auto default_delay = (_network.address() == sysx::ipv4_addr{127,0,0,1}) ? 0 : 2;
			const auto start_delay = sysx::this_process::getenv("START_DELAY", default_delay);
			discoverer_type* master = new discoverer_type(_network, _port);
			master->id(sysx::to_host_format(_network.address().rep()));
			this_server.factory()->instances().register_instance(master);
			master->after(std::chrono::seconds(start_delay));
//			master->at(Kernel::Time_point(std::chrono::seconds(start_time)));
			this_server.timer_server()->send(master);

//			if (_network.address() == traits_type::localhost()) {
//				schedule_pingpong_after(std::chrono::seconds(0), this_server);
//			}

			if (_network.address() == _masteraddr) {
				schedule_autoreg_app(this_server);
			}

			schedule_shutdown_after(std::chrono::seconds(_timeout), master, this_server);
		}
	}

private:

	template<class Time>
	void
	schedule_pingpong_after(Time delay, Server& this_server) {
		Ping_pong* p = new Ping_pong(_numpings);
		p->after(delay);
		this_server.timer_server()->send(p);
	}

	template<class Time>
	void
	schedule_shutdown_after(Time delay, discoverer_type* master, Server& this_server) {
		Delayed_shutdown<addr_type>* shutdowner = new Delayed_shutdown<addr_type>(master->hierarchy(), _normal);
		shutdowner->after(delay);
		shutdowner->parent(this);
		this_server.timer_server()->send(shutdowner);
	}

	void
	schedule_autoreg_app(Server& this_server) {
		Autoreg_app* app = new Autoreg_app;
		app->after(std::chrono::seconds(5));
		this_server.timer_server()->send(app);
	}

	void
	parse_cmdline_args(Server& this_server) {
		try {
			_cmdline.parse();
			if (!_network) {
				throw sysx::invalid_cmdline_argument("--network");
			}
			if (_role != role_slave) {
				throw sysx::invalid_cmdline_argument("--role");
			}
		} catch (sysx::invalid_cmdline_argument& err) {
			std::cerr << err.what() << ": " << err.arg() << std::endl;
			this_server.factory()->set_exit_code(1);
		}
	}

	std::string _role;
	sysx::network<sysx::ipv4_addr> _network;
	sysx::port_type _port;
	uint32_t _numpings = 10;
	uint32_t _timeout = 60;
	bool _normal = true;
	sysx::ipv4_addr _masteraddr;
	sysx::cmdline _cmdline;

};

template<class Address>
struct Hosts: public std::vector<Address> {

	typedef Address addr_type;

	friend std::istream&
	operator>>(std::istream& cmdline, Hosts& rhs) {
		std::string filename;
		cmdline >> filename;
		std::clog << "reading hosts from " << filename << std::endl;
		std::ifstream in(filename);
		rhs.clear();
		std::copy(
			std::istream_iterator<addr_type>(in),
			std::istream_iterator<addr_type>(),
			std::back_inserter(rhs)
		);
		return cmdline;
	}

};

int main(int argc, char* argv[]) {

	sysx::port_type discovery_port = 54321;
	const uint32_t do_not_kill = std::numeric_limits<uint32_t>::max();
	uint32_t kill_after = do_not_kill;
	uint32_t kill_timeout = do_not_kill;
	Hosts<sysx::ipv4_addr> hosts2;
	sysx::ipv4_addr master_addr{127,0,0,1};
	sysx::ipv4_addr kill_addr{127,0,0,2};

	typedef stdx::log<decltype(main)> this_log;
	int retval = 0;
	sysx::network<sysx::ipv4_addr> network;
	size_t nhosts = 0;
	std::string role = role_master;
	try {
		sysx::cmdline cmd(argc, argv, {
			sysx::cmd::ignore_first_arg(),
			sysx::cmd::ignore_arg("--normal"),
			sysx::cmd::make_option({"--hosts"}, hosts2),
			sysx::cmd::make_option({"--network"}, network),
			sysx::cmd::make_option({"--num-hosts"}, nhosts),
			sysx::cmd::make_option({"--role"}, role),
			sysx::cmd::make_option({"--port"}, discovery_port),
			sysx::cmd::make_option({"--timeout"}, kill_timeout),
			sysx::cmd::make_option({"--master"}, master_addr),
			sysx::cmd::make_option({"--kill"}, kill_addr),
			sysx::cmd::make_option({"--kill-after"}, kill_after)
		});
		cmd.parse();
		if (role != role_master and role != role_slave) {
			throw sysx::invalid_cmdline_argument("--role");
		}
		if (!network) {
			throw sysx::invalid_cmdline_argument("--network");
		}
	} catch (sysx::invalid_cmdline_argument& err) {
		std::cerr << err.what() << ": " << err.arg() << std::endl;
		return 1;
	}

	if (role == role_master) {

		this_log() << "Network = " << network << std::endl;
		this_log() << "Num peers = " << nhosts << std::endl;
		this_log() << "Role = " << role << std::endl;
		this_log() << "start,mid = " << *network.begin() << ',' << *network.middle() << std::endl;

		std::vector<sysx::endpoint> hosts;
		if (network) {
			std::transform(
				network.begin(),
				network.begin() + nhosts,
				std::back_inserter(hosts),
				[discovery_port] (const sysx::ipv4_addr& addr) {
					return sysx::endpoint(addr, discovery_port);
				}
			);
		} else {
			std::transform(
				hosts2.begin(),
				hosts2.begin() + std::min(hosts2.size(), nhosts),
				std::back_inserter(hosts),
				[discovery_port] (const sysx::ipv4_addr& addr) {
					return sysx::endpoint(addr, discovery_port);
				}
			);
		}
		springy::Springy_graph graph;
		graph.add_nodes(hosts.begin(), hosts.end());

		sysx::process_group procs;
		for (sysx::endpoint endpoint : hosts) {
			procs.emplace([endpoint, &argv, nhosts, &network, discovery_port, master_addr, kill_addr, kill_after] () {
				char workdir[PATH_MAX];
				::getcwd(workdir, PATH_MAX);
				uint32_t timeout = 60;
				bool normal = true;
				if (endpoint.addr4() == kill_addr) {
					timeout = kill_after;
					normal = false;
				}
				return sysx::this_process::execute(
					#if defined(FACTORY_TEST_USE_SSH)
					"/usr/bin/ssh", endpoint.addr4(), "cd", workdir, ";", "exec",
					#endif
					argv[0],
					"--network", sysx::network<sysx::ipv4_addr>(endpoint.addr4(), network.netmask()),
					"--port", discovery_port,
					"--role", "slave",
					"--timeout", timeout,
					"--normal", normal,
					"--master", master_addr
				);
			});
		}

		this_log() << "Forked " << procs << std::endl;
		retval = procs.wait();

	} else {
		using namespace factory;
		retval = factory_main<Main<sysx::ipv4_addr>,config>(argc, argv);
	}

	return retval;
}
