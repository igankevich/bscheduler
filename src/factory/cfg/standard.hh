#ifndef FACTORY_CFG_STANDARD_HH
#define FACTORY_CFG_STANDARD_HH

namespace factory {

	inline namespace standard_config {

		struct config {
			typedef components::Managed_object<components::Server<config>> server;
			typedef components::Principal<config> kernel;
			typedef components::CPU_server<config> local_server;
			typedef components::NIC_server<config, factory::unix::socket> remote_server;
			typedef components::Timer_server<config> timer_server;
			typedef components::Sub_Iserver<config> app_server;
			typedef components::Principal_server<config> principal_server;
			typedef components::NIC_server<config, components::Web_socket> external_server;
			typedef components::Basic_factory<config> factory;
		};

		template<class T>
			using Identifiable = components::Identifiable<T>;
		
		template<class T>
			using Mobile = components::Type_init<T, config::kernel>;

		template<class T>
			using Unidentifiable = components::Type_init<T, config::kernel>;

		typedef config::kernel Kernel;
		typedef config::server Server;
		typedef components::Type<config::kernel> Type;
	}

}

#endif // FACTORY_CFG_STANDARD_HH
