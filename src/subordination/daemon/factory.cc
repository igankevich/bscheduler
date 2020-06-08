#include <subordination/daemon/config.hh>
#include <subordination/daemon/factory.hh>

sbnd::Factory::Factory(): Factory(sys::thread_concurrency()) {}

sbnd::Factory::Factory(unsigned concurrency): _local(concurrency) {
    this->_local.name("upstrm");
    this->_remote.name("nic");
    this->_remote.native_pipeline(&this->_local);
    this->_remote.foreign_pipeline(&this->_process);
    this->_remote.remote_pipeline(&this->_remote);
    this->_remote.set_other_mutex(this->_process.mutex());
    this->_remote.types(&this->_types);
    this->_remote.instances(&this->_instances);
    this->_remote.transactions(&this->_transactions);
    #if !defined(SUBORDINATION_PROFILE_NODE_DISCOVERY)
    this->_process.name("proc");
    this->_process.set_other_mutex(this->_remote.mutex());
    this->_process.native_pipeline(&this->_local);
    this->_process.foreign_pipeline(&this->_remote);
    this->_process.remote_pipeline(&this->_remote);
    this->_process.types(&this->_types);
    this->_process.instances(&this->_instances);
    this->_process.transactions(&this->_transactions);
    this->_unix.name("unix");
    this->_unix.native_pipeline(&this->_local);
    this->_unix.foreign_pipeline(&this->_process);
    this->_unix.remote_pipeline(&this->_remote);
    this->_unix.types(&this->_types);
    this->_unix.instances(&this->_instances);
    this->_unix.transactions(&this->_transactions);
    #endif
    this->_transactions.pipelines({&this->_remote,&this->_process,&this->_unix});
    //this->_transactions.open(SUBORDINATION_SHARED_STATE_DIR "/transactions");
    this->_transactions.open("transactions");
}

sbnd::Factory sbnd::factory{};
