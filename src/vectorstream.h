#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <streambuf>
#include <string_view>
#include <vector>

namespace mtar
{
	template<typename CharT, typename Traits = std::char_traits<CharT>, typename Allocator = std::allocator<CharT>>
	class basic_vectorbuf : public std::basic_streambuf<CharT, Traits>
	{
	private:
		using base = std::basic_streambuf<CharT, Traits>;

	public:
		using char_type = typename base::char_type;
		using traits_type = typename base::traits_type;
		using int_type = typename base::int_type;
		using pos_type = typename base::pos_type;
		using off_type = typename base::off_type;

		basic_vectorbuf() {}
		basic_vectorbuf(const Allocator& alloc) : v(alloc) {}
		template<typename InputIt>
		basic_vectorbuf(InputIt first, InputIt last, const Allocator& alloc = Allocator()) : v(first, last, alloc) {}
		basic_vectorbuf(const std::vector<CharT, Allocator>& v_) : v(v_, alloc) {}
		basic_vectorbuf(const std::vector<CharT, Allocator>& v_, const Allocator& alloc) : v(v_, alloc) {}
		basic_vectorbuf(std::vector<CharT, Allocator>&& v_) : v(std::move(v_)) {}
		basic_vectorbuf(std::vector<CharT, Allocator>&& v_, const Allocator& alloc) : v(std::move(v_), alloc) {}
		basic_vectorbuf(std::initializer_list<CharT> init, const Allocator& alloc = Allocator()) : v(init, alloc) {}

		std::vector<CharT, Allocator> vec_copy() const
		{
			return { v.begin(), v.begin() + v_size };
		}

		// note: this view will no longer be valid after reallocation
		std::basic_string_view<CharT, Traits> view() const
		{
			return { v.begin(), v.begin() + v_size };
		}

		std::size_t size() const
		{
			return v_size;
		}

		const CharT& operator[](std::size_t pos) const
		{
			return v[pos];
		}

		auto begin()
		{
			return v.begin();
		}

		auto end()
		{
			return v.begin() + v_size + 1;
		}

		void swap(std::vector<CharT, Allocator>& v_)
		{
			v.swap(v_);
			pos_in = 0;
			pos_out = 0;
			v_size = v.size();
		}

		template<typename Allocator2>
		void copy_from(const std::vector<CharT, Allocator>& v_)
		{
			v.resize(v_.size());
			std::copy(v_.begin(), v_.end(), v.begin());
			pos_in = 0;
			pos_out = 0;
			v_size = v.size();
		}

		template<typename InputIt>
		void copy_from(InputIt first, InputIt last)
		{
			v.clear();
			std::copy(first, last, std::back_inserter(v))
			pos_in = 0;
			pos_out = 0;
			v_size = v.size();
		}

		template<typename InputIt, typename Size>
		void copy_from_n(InputIt first, Size size_)
		{
			v.resize(size_);
			std::copy_n(first, size_, v.begin());
			pos_in = 0;
			pos_out = 0;
			v_size = size_;
		}

		template<typename OutputIt>
		void copy_to(OutputIt out) const
		{
			std::copy_n(v.begin(), v_size, out);
		}

	private:
		std::vector<CharT, Allocator> v;
		std::size_t pos_in = 0, pos_out = 0;
		std::size_t v_size = 0;

		void update_get_area()
		{
			setg(v.data(), v.data() + pos_in, v.data() + v.size());
		}

		void update_put_area()
		{
			setp(v.data() + pos_out, v.data() + v.size());
		}

		void expand_realloc(size_t least = 0)
		{
			size_t new_size = v.size() * 2;
			if (new_size == 0)
			{
				new_size = 1;
			}
			while (new_size < least)
			{
				new_size *= 2;
			}
			v.resize(new_size);
		}
		
	protected:
		using std::basic_streambuf<CharT, Traits>::setg;
		using std::basic_streambuf<CharT, Traits>::setp;

		virtual basic_vectorbuf<CharT, Traits, Allocator>* setbuf(char_type* s, std::streamsize n) override
		{
			v = std::vector(s, s + n);
			return this;
		}

		virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override
		{
			// neither in nor out is set
			if ((which & std::ios_base::in) != std::ios_base::in && (which & std::ios_base::out) != std::ios_base::out)
			{
				return -1;
			}
			switch (dir)
			{
			case std::ios_base::beg:
				// out of range
				if (off < 0 || off >= v.size())
				{
					return -1;
				}
				if ((which & std::ios_base::in) == std::ios_base::in)
				{
					pos_in = off;
					update_get_area();
				}
				if ((which & std::ios_base::out) == std::ios_base::out)
				{
					pos_out = off;
					update_put_area();
				}
				return off;
			case std::ios_base::end:
				// out of range
				if (v.size() < -off || off > 0)
				{
					return -1;
				}
				if ((which & std::ios_base::in) == std::ios_base::in)
				{
					pos_in = v.size() + off;
					update_get_area();
				}
				if ((which & std::ios_base::out) == std::ios_base::out)
				{
					pos_out = v.size() + off;
					update_put_area();
				}
				return v.size() + off;
			case std::ios_base::cur:
				// don't allow setting both positions while using cur (same as std::basic_stringbuf)
				if ((which & (std::ios_base::in | std::ios_base::out)) == (std::ios_base::in | std::ios_base::out))
				{
					return -1;
				}
				if ((which & std::ios_base::in) == std::ios_base::in)
				{
					// out of range
					if (pos_in < -off || pos_in + off > v.size())
					{
						return -1;
					}
					pos_in += off;
					update_get_area();
					return pos_in;
				}
				if ((which & std::ios_base::out) == std::ios_base::out)
				{
					// out of range
					if (pos_out < -off || pos_out + off > v.size())
					{
						return -1;
					}
					pos_out += off;
					update_put_area();
					return pos_out;
				}
			}
			return -1;
		}

		virtual pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out) override
		{
			if ((which & std::ios_base::in) != std::ios_base::in && (which & std::ios_base::out) != std::ios_base::out)
			{
				return -1;
			}
			if ((which & std::ios_base::in) == std::ios_base::in)
			{
				pos_in = pos;
				update_get_area();
			}
			if ((which & std::ios_base::out) == std::ios_base::out)
			{
				pos_out = pos;
				update_put_area();
			}
			return pos;
		}

		virtual std::streamsize showmanyc() override
		{
			std::streamsize num = v.size() - pos_in;
			if (num == 0)
			{
				return -1;
			}
			return num;
		}

		virtual int_type underflow() override
		{
			if (pos_in == v.size())
			{
				return Traits::eof();
			}
			return Traits::to_int_type(v[pos_in]); // no increment
		}

		virtual std::streamsize xsgetn(char_type* s, std::streamsize count) override
		{
			if (pos_in == v.size())
			{
				return Traits::eof();
			}
			const std::streamsize num = std::min(count, std::streamsize(v.size() - pos_in));
			std::copy_n(v.begin() + pos_in, num, s);
			pos_in += num;
			update_get_area();
			return num;
		}

		virtual std::streamsize xsputn(const char_type* s, std::streamsize count) override
		{
			if (pos_out + count > v.size())
			{
				expand_realloc(pos_out + count);
			}
			std::copy_n(s, count, v.begin() + pos_out);
			pos_out += count;
			if (pos_out > v_size)
			{
				v_size = pos_out;
			}
			update_put_area();
			return count;
		}

		virtual int_type overflow(int_type ch = Traits::eof()) override
		{
			if (Traits::eq_int_type(ch, Traits::eof()))
			{
				if (pos_out + 1 > v.size())
				{
					expand_realloc();
				}
				v[pos_out] = ch;
				pos_out++;
				if (pos_out > v_size)
				{
					v_size = pos_out;
				}
				update_put_area();
			}
			return Traits::not_eof(ch);
		}

		virtual int_type pbackfail(int_type ch = Traits::eof()) override
		{
			if (Traits::eq_int_type(ch, Traits::eof()))
			{
				pos_in--;
				update_put_area();
			}
			else
			{
				v[pos_in - 1] = ch;
			}
			return Traits::not_eof(ch);
		}
	};

	template<typename CharT, typename Traits = std::char_traits<CharT>, typename Allocator = std::allocator<CharT>>
	class basic_vectorstream : public std::basic_iostream<CharT, Traits>
	{
	private:
		using base = std::basic_iostream<CharT, Traits>;
		using vectorbuf = basic_vectorbuf<CharT, Traits, Allocator>;
		vectorbuf* buf;

	public:
		// this feels illegal
		basic_vectorstream(basic_vectorbuf<CharT, Traits, Allocator>& vb) : base(buf = &vb) {}
		basic_vectorstream() : base(buf = new vectorbuf()) {}
		basic_vectorstream(const Allocator& alloc) : base(buf = new vectorbuf(alloc)) {}
		template<typename InputIt>
		basic_vectorstream(InputIt first, InputIt last, const Allocator& alloc = Allocator()) : base(buf = new vectorbuf(first, last, alloc)) {}
		basic_vectorstream(const std::vector<CharT, Allocator>& v_) : base(buf = new vectorbuf(v_, alloc)) {}
		basic_vectorstream(const std::vector<CharT, Allocator>& v_, const Allocator& alloc) : base(buf = new vectorbuf(v_, alloc)) {}
		basic_vectorstream(std::vector<CharT, Allocator>&& v_) : base(buf = new vectorbuf(std::move(v_))) {}
		basic_vectorstream(std::vector<CharT, Allocator>&& v_, const Allocator& alloc) : base(buf = new vectorbuf(std::move(v_), alloc)) {}
		basic_vectorstream(std::initializer_list<CharT> init, const Allocator& alloc = Allocator()) : base(buf = new vectorbuf(init, alloc)) {}
		~basic_vectorstream()
		{
			delete buf;
		}

		std::vector<CharT, Allocator> vec_copy() const
		{
			return buf->(v.begin(), v.begin() + size);
		}

		// note: this view will no longer be valid after reallocation
		std::basic_string_view<CharT, Traits> view() const
		{
			return buf->(v.begin(), v.begin() + size);
		}

		std::size_t size() const
		{
			return buf->size;
		}

		const CharT& operator[](std::size_t pos) const
		{
			return buf->v[pos];
		}

		void swap(std::vector<CharT, Allocator>& v)
		{
			buf->swap(v);
		}

		template<typename Allocator2>
		void copy_from(const std::vector<CharT, Allocator>& v)
		{
			buf->copy_from(v);
		}

		template<typename InputIt>
		void copy_from(InputIt first, InputIt last)
		{
			buf->copy_from(first, last);
		}

		template<typename InputIt, typename Size>
		void copy_from_n(InputIt first, Size size_)
		{
			buf->copy_from(first, size);
		}

		template<typename OutputIt>
		void copy_to(OutputIt out) const
		{
			buf->copy_to(out);
		}
	};

	using vectorstream = basic_vectorstream<char>;
}
