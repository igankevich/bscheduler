#include <iostream>
#include <iomanip>

#include <sysx/sharedmem.hh>
#include <sysx/shmembuf.hh>

#include "test.hh"

// disable logs
namespace stdx {

	template<>
	struct disable_log_category<sysx::buffer_category>:
	public std::integral_constant<bool, true> {};

}

template<class T>
struct Test_shmem: public test::Test<Test_shmem<T>> {

	typedef stdx::log<Test_shmem> this_log;
	typedef typename sysx::shared_mem<T>::size_type size_type;
	typedef sysx::shared_mem<T> shmem;
	typedef sysx::basic_shmembuf<T> shmembuf;

	static bool
	shmem_invariant(shmem& shm) {
		return shm.begin() != nullptr && shm.end() != nullptr;
	}

	void xrun() override {
		const typename sysx::shared_mem<T>::size_type SHMEM_SIZE = 512;
		sysx::shared_mem<T> mem1(SHMEM_SIZE, 0666);
		sysx::shared_mem<T> mem2(mem1.id());
		test::invar(shmem_invariant, mem1);
		test::invar(shmem_invariant, mem2);
		const size_type real_size = mem1.size();
		test::equal(mem1.size(), real_size, "bad size: mem1=", std::cref(mem1));
		test::equal(mem2.size(), real_size, "bad size: mem2=", std::cref(mem2));
//		mem2.sync();
//		test::equal(mem2.size(), real_size, "bad size after sync: mem2=", std::cref(mem2));
//		mem1.resize(real_size * 2);
//		const size_type new_size = mem1.size();
//		test::equal(mem1.size(), new_size, "bad size after resize: mem1=", std::cref(mem1));
//		mem2.sync();
//		test::equal(mem2.size(), new_size, "bad size after sync: mem2=", std::cref(mem2));
		std::generate(mem1.begin(), mem1.end(), test::randomval<T>);
		test::compare(mem1, mem2);
	}

};

template<class T>
struct Test_shmembuf: public test::Test<Test_shmembuf<T>> {

	typedef stdx::log<Test_shmembuf> this_log;
	typedef typename sysx::shared_mem<T>::size_type size_type;
	typedef sysx::shared_mem<T> shmem;
	typedef sysx::basic_shmembuf<T> shmembuf;

	void xrun() override {
		shmembuf buf1("/test-shmem-2", 0600);
		shmembuf buf2("/test-shmem-2");
		for (int i=0; i<12; ++i) {
			const size_t size = 2u << i;
			this_log() << "test_shmembuf() middle: size=" << size << std::endl;
			std::vector<T> input(size);
			std::generate(input.begin(), input.end(), test::randomval<T>);
			buf1.lock();
			buf1.sputn(&input.front(), input.size());
			buf1.unlock();
			std::vector<T> output(input.size());
			buf2.lock();
			buf2.sgetn(&output.front(), output.size());
			buf2.unlock();
			test::compare(input, output);
		}
	}

//	void test_shmembuf_pipe() {
//		std::string content = "Hello world";
//		std::stringstream src;
//		src << content;
//		shmembuf buf("/test-shmem-3", 0600);
//		buf.lock();
//		buf.fill_from(*src.rdbuf());
//		buf.unlock();
//		std::stringstream result;
//		buf.flush_to(*result.rdbuf());
//		test::equal(result.str(), content);
//	}

};

int main(int argc, char* argv[]) {
	test::Test_suite tests{"Test suite: shared memory", {
		new Test_shmem<char>,
		new Test_shmem<unsigned char>,
		new Test_shmembuf<char>
	}};
	return tests.run();
}