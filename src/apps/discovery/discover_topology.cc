#include <map>
#include <unordered_map>
#include <set>
#include <vector>
#include <algorithm>
#include <iterator>
#include <fstream>

#include <stdx/log.hh>

#include <sysx/endpoint.hh>
#include <sysx/cmdline.hh>

#include "network.hh"
#include "location.hh"
#include "csv_tuple.hh"

template<class T, class Iterator>
T
encode_as_number(Iterator first, Iterator last) {
	constexpr const T radix('Z' - 'A' + 1);
	T mult(1);
	T result(0);
	while (first != last) {
		result += T(std::toupper(*first)-'A') * mult;
		mult *= radix;
		++first;
	}
	return result;
}

template<class T>
std::string
decode_as_string(T number) {
	constexpr const T radix('Z' - 'A' + 1);
	std::string result;
	while (number) {
		const T rem = number % radix;
		number /= radix;
		result.push_back('A' + rem);
	}
	return result;
}

template<class Addr>
struct Topology {

	typedef Addr addr_type;
	typedef discovery::Network<addr_type> network_type;
	typedef typename network_type::rep_type uint_type;
	typedef float float_type;
	typedef uint32_t city_type;
	typedef uint32_t country_type;
	typedef std::unordered_map<city_type,country_type> countrymap_type;
	typedef std::set<discovery::City> locations_type;
	typedef std::unordered_map<city_type,uint_type> hostmap_type;
	typedef std::multiset<discovery::Location> locationset_type;
	typedef std::unordered_map<country_type,locationset_type> locationmap_type;
	typedef stdx::log<Topology> this_log;

	Topology(const std::string& location_file, const std::string& block_file, const std::string& country) {
		load_countries(location_file, country);
		load_sub_networks(block_file);
	}

	void
	load_countries(const std::string& filename, const std::string& filter_country) {
		this_log() << "Reading locations from " << filename << std::endl;
		using namespace discovery;
		typedef csv_tuple<',',
			city_type,
			ignore_field,
			ignore_field,
			ignore_field,
			std::string
		> mytuple;
		std::ifstream in(filename);
		in.ignore(1024*1024, '\n');
		std::for_each(
			std::istream_iterator<mytuple>(in),
			std::istream_iterator<mytuple>(),
			[this,&filter_country] (const mytuple& rhs) {
				const city_type city = std::get<0>(rhs);
				const std::string country_str = std::get<4>(rhs);
				if (not filter_country.empty() and country_str == filter_country) {
					const country_type country = encode_as_number<country_type>(country_str.begin(), country_str.end());
					countries.emplace(city, country);
//					std::clog << country_str << std::endl;
				}
			}
		);
		this_log() << "Total no. of cities = " << countries.size() << std::endl;
	}

	void load_sub_networks(const std::string& filename) {
		this_log() << "Reading sub-networks from " << filename << std::endl;
		using namespace discovery;
//		typedef std::tuple<country_type,city_type,float_type,float_type> location_tuple;
		typedef csv_tuple<',',
			network_type,
			city_type,
			country_type,
			ignore_field,
			ignore_field,
			ignore_field,
			ignore_field,
			float_type,
			float_type
		> mytuple;
		hostmap_type nhosts;
		locations_type locs;
//		load_countries(countries);
		std::ifstream in(filename);
		in.ignore(1024*1024, '\n');
		std::for_each(
			std::istream_iterator<mytuple>(in),
			std::istream_iterator<mytuple>(),
			[&locs,&nhosts,this] (const mytuple& rhs) {
				const network_type net = std::get<0>(rhs);
				const city_type city = std::get<1>(rhs);
				const float_type lat = std::get<7>(rhs);
				const float_type lon = std::get<8>(rhs);
				const country_type country = countries[city];
				if (city and country) {
					nhosts[city] += net.count();
					locs.emplace(City{country, city, lat, lon});
				}
			}
		);
		this_log() << "No. of cities with at least one sub-network = " << nhosts.size() << std::endl;
		std::for_each(
			locs.begin(),
			locs.end(),
			[this,&nhosts] (const City& rhs) {
//				const country_type country = std::get<0>(rhs);
//				const city_type city = std::get<1>(rhs);
//				const float_type lat = std::get<2>(rhs);
//				const float_type lon = std::get<3>(rhs);
				sorted_locs[rhs._country].emplace(rhs._city, rhs._latitude, rhs._longitude, nhosts[rhs._city]);
			}
		);

	}

	void
	save() {
		using namespace discovery;
		std::ofstream vectors("vectors.dat");
		std::ofstream graph("graph.dat");
		std::for_each(
			sorted_locs.begin(),
			sorted_locs.end(),
			[&graph,&vectors] (const typename locationmap_type::value_type& rhs) {
				this_log() << "No. of cities with at least one sub-network in "
					<< decode_as_string(rhs.first) << " = " << rhs.second.size() << std::endl;
				const auto last = rhs.second.end();
				for (auto it=rhs.second.begin(); it!=last; ++it) {
					const Location& loc_a = *it;
					auto first = it;
					++first;
//					auto first = rhs.second.begin();
					auto result = std::min_element(
						first,
						last,
						[&loc_a] (const Location& lhs, const Location& rhs) {
							return lhs != loc_a and discovery::distance(loc_a, lhs)
								< discovery::distance(loc_a, rhs);
						}
					);
					const Location& loc_b = *result;
					if (result != last) {
						graph << loc_a << '\n' << loc_b << "\n\n\n";
						vectors << loc_a.point() << ' ' << (loc_b.point() - loc_a.point())*float_type(2)/float_type(3) << '\n';
					}
				}
			}
		);
	}

private:

	countrymap_type countries;
	locationmap_type sorted_locs;

};

int
main(int argc, char* argv[]) {

	int retval = 0;

	std::string
		country = "",
		locations = "GeoLite2-City-Locations-en.csv",
		blocks = "GeoLite2-City-Blocks-IPv4.csv";

	sysx::cmdline cmd(argc, argv, {
		sysx::cmd::ignore_first_arg(),
		sysx::cmd::make_option({"--country"}, country),
		sysx::cmd::make_option({"--locations"}, locations),
		sysx::cmd::make_option({"--blocks"}, blocks),
	});

	try {
		cmd.parse();
	} catch (sysx::invalid_cmdline_argument& err) {
		std::cerr << err.what() << ": " << err.arg() << std::endl;
		retval = -1;
	}

	if (retval == 0) {
		Topology<sysx::ipv4_addr> topo(locations, blocks, country);
		topo.save();
	}

	return retval;
}
