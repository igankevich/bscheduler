#include <subordination/daemon/small_factory.hh>

sbnc::Factory::Factory() {
    this->_local.name("sbnc local");
    this->_local.error_pipeline(&this->_remote);
    this->_remote.name("sbnc remote");
    this->_remote.native_pipeline(&this->_local);
    this->_remote.foreign_pipeline(&this->_remote);
    this->_remote.remote_pipeline(&this->_remote);
    this->_remote.types(&this->_types);
}

void sbnc::Factory::start() {
    this->_local.start(),
    this->_remote.start();
}

void sbnc::Factory::stop() {
    this->_local.stop(),
    this->_remote.stop();
}

void sbnc::Factory::wait() {
    this->_local.wait(),
    this->_remote.wait();
}

void sbnc::Factory::clear() {
    sbn::kernel_sack sack;
    this->_local.clear(sack),
    this->_remote.clear(sack);
}

sbnc::Factory sbnc::factory;
