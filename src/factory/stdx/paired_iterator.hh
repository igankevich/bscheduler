#ifndef FACTORY_STDX_PAIRED_ITERATOR_HH
#define FACTORY_STDX_PAIRED_ITERATOR_HH

#include <iterator>
#include <utility>

namespace factory {

	namespace stdx {

		template<class T1, class T2>
		struct pair: public std::pair<T1,T2> {

			typedef std::pair<T1,T2> base_pair;

			explicit
			pair(T1 x, T2 y):
				base_pair(x, y) {}

			pair(const pair& rhs):
				base_pair(rhs) {}

			pair& operator=(pair&& rhs) {
				base_pair::first = std::move(rhs.first);
				base_pair::second = std::move(rhs.second);
				return *this;
			}
		};

		template<class It1, class It2>
		struct paired_iterator: public std::iterator<
			typename std::iterator_traits<It1>::iterator_category,
			std::tuple<typename std::iterator_traits<It1>::value_type,typename std::iterator_traits<It2>::value_type>,
			std::ptrdiff_t,
			stdx::pair<typename std::iterator_traits<It1>::value_type*,typename std::iterator_traits<It2>::value_type*>,
			stdx::pair<typename std::iterator_traits<It1>::reference,typename std::iterator_traits<It2>::reference>
			>
		{
			typedef typename std::iterator_traits<paired_iterator>::reference reference;
			typedef typename std::iterator_traits<paired_iterator>::pointer pointer;
			typedef typename std::iterator_traits<paired_iterator>::difference_type difference_type;
			typedef const reference const_reference;
			typedef const pointer const_pointer;

			typedef typename std::iterator_traits<It1>::value_type&& rvalueref1;
			typedef typename std::iterator_traits<It2>::value_type&& rvalueref2;
			typedef stdx::pair<
				typename std::iterator_traits<It1>::reference,
				typename std::iterator_traits<It2>::reference>
					pair_type;

			inline paired_iterator(It1 x, It2 y): iter1(x), iter2(y) {}
			inline paired_iterator() = default;
			inline ~paired_iterator() = default;
			inline paired_iterator(const paired_iterator&) = default;
			inline paired_iterator& operator=(const paired_iterator&) = default;

			constexpr bool operator==(const paired_iterator& rhs) const { return iter1 == rhs.iter1; }
			constexpr bool operator!=(const paired_iterator& rhs) const { return !this->operator==(rhs); }

			inline const_reference
			operator*() const {
				return pair_type(*iter1,*iter2);
			}

			inline reference
			operator*() {
				return pair_type(*iter1,*iter2);
			}

			inline const_pointer operator->() const { return std::make_pair(iter1.operator->(),iter2.operator->()); }
			inline pointer operator->() { return std::make_pair(iter1.operator->(),iter2.operator->()); }
			inline paired_iterator& operator++() { ++iter1; ++iter2; return *this; }
			inline paired_iterator operator++(int) { paired_iterator tmp(*this); ++iter1; ++iter2; return tmp; }
			inline difference_type operator-(const paired_iterator& rhs) const {
				return std::distance(rhs.iter1, iter1);
			}
			inline paired_iterator operator+(difference_type rhs) const {
				return paired_iterator(std::advance(iter1, rhs), std::advance(iter2, rhs));
			}
			inline It1 first() const { return iter1; }
			inline It2 second() const { return iter2; }

		private:
			It1 iter1;
			It2 iter2;
		};

		template<class It1, class It2>
		inline paired_iterator<It1,It2>
		make_paired(It1 it1, It2 it2) {
			return paired_iterator<It1,It2>(it1, it2);
		}
	
	}

}

#endif // FACTORY_STDX_PAIRED_ITERATOR_HH
