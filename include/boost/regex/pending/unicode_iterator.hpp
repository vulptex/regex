/*
 *
 * Copyright (c) 2004
 * Dr John Maddock
 *
 * Use, modification and distribution are subject to the 
 * Boost Software License, Version 1.0. (See accompanying file 
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */
 
 /*
  *   LOCATION:    see http://www.boost.org for most recent version.
  *   FILE         unicode_iterator.hpp
  *   VERSION      see <boost/version.hpp>
  *   DESCRIPTION: Iterator adapters for converting between different Unicode encodings.
  */

#ifndef BOOST_REGEX_UNICODE_ITERATOR_HPP
#define BOOST_REGEX_UNICODE_ITERATOR_HPP
#include <boost/cstdint.hpp>
#include <boost/assert.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/static_assert.hpp>
#include <stdexcept>
#ifndef BOOST_NO_STD_LOCALE
#include <sstream>
#endif

namespace boost{

namespace detail{

static const ::boost::uint16_t high_surrogate_base = 0xD7C0u;
static const ::boost::uint16_t low_surrogate_base = 0xDC00u;
static const ::boost::uint32_t ten_bit_mask = 0x3FFu;

inline bool is_high_surrogate(::boost::uint16_t v)
{
   return (v & 0xFC00u) == 0xd800u;
}
inline bool is_low_surrogate(::boost::uint16_t v)
{
   return (v & 0xFC00u) == 0xdc00u;
}
template <class T>
inline bool is_surrogate(T v)
{
   return (v & 0xF800u) == 0xd800;
}

inline unsigned utf8_byte_count(boost::uint8_t c)
{
   // if the most significant bit with a zero in it is in position
   // 8-N then there are N bytes in this UTF-8 sequence:
   boost::uint8_t mask = 0x80u;
   unsigned result = 0;
   while(c & mask)
   {
      ++result;
      mask >>= 1;
   }
   return (result == 0) ? 1 : result;
}

inline unsigned utf8_trailing_byte_count(boost::uint8_t c)
{
   return utf8_byte_count(c) - 1;
}

}

template <class BaseIterator, class U16Type = ::boost::uint16_t>
class u32_to_u16_iterator
   : public boost::iterator_facade<u32_to_u16_iterator<BaseIterator, U16Type>, U16Type, std::bidirectional_iterator_tag, const U16Type>
{
   typedef boost::iterator_facade<u32_to_u16_iterator<BaseIterator, U16Type>, U16Type, std::bidirectional_iterator_tag, const U16Type> base_type;

#if !defined(BOOST_NO_STD_ITERATOR_TRAITS) && !defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION)
   typedef typename std::iterator_traits<BaseIterator>::value_type base_value_type;

   BOOST_STATIC_ASSERT(sizeof(base_value_type)*CHAR_BIT == 32);
   BOOST_STATIC_ASSERT(sizeof(U16Type)*CHAR_BIT == 16);
#endif

public:
   typename base_type::reference
      dereference()const
   {
      if(m_current == 2)
         extract_current();
      return m_values[m_current];
   }
   bool equal(const u32_to_u16_iterator& that)const
   {
      if(m_position == that.m_position)
      {
         // Both m_currents must be equal, or both even
         // this is the same as saying their sum must be even:
         return (m_current + that.m_current) & 1u ? false : true;
            /*
         if((m_current >= 2) && (that.m_current < 2))
            const_cast<u32_to_u16_iterator*>(this)->extract_current();
         else if((m_current < 2) && (that.m_current >= 2))
            const_cast<u32_to_u16_iterator&>(that).extract_current();
         if(m_current == that.m_current)
            return true;
            */
      }
      return false;
   }
   void increment()
   {
      // if we have a pending read then read now, so that we know whether
      // to skip a position, or move to a low-surrogate:
      if(m_current == 2)
      {
         // pending read:
         extract_current();
      }
      // move to the next surrogate position:
      ++m_current;
      // if we've reached the end skip a position:
      if(m_values[m_current] == 0)
      {
         m_current = 2;
         ++m_position;
      }
   }
   void decrement()
   {
      if(m_current != 1)
      {
         // decrementing an iterator always leads to a valid position:
         --m_position;
         extract_current();
         m_current = m_values[1] ? 1 : 0;
      }
      else
      {
         m_current = 0;
      }
   }
   BaseIterator base()const
   {
      return m_position;
   }
   // construct:
   u32_to_u16_iterator() : m_position(), m_current(0)
   {
      m_values[2] = 0;
   }
   u32_to_u16_iterator(BaseIterator b) : m_position(b), m_current(2)
   {
      m_values[2] = 0;
   }
private:
   static void invalid_code_point(::boost::uint32_t val)
   {
#ifndef BOOST_NO_STD_LOCALE
      std::stringstream ss;
      ss << "Invalid UTF-32 code point U+" << std::showbase << std::hex << val << " encountered while trying to encode UTF-16 sequence";
      std::out_of_range e(ss.str());
#else
      std::out_of_range e("Invalid UTF-32 code point encountered while trying to encode UTF-16 sequence");
#endif
      boost::throw_exception(e);
   }

   void extract_current()const
   {
      // begin by checking for a code point out of range:
      ::boost::uint32_t v = *m_position;
      if(v >= 0x10000u)
      {
         if(v > 0x10FFFFu)
            invalid_code_point(*m_position);
         // split into two surrogates:
         m_values[0] = static_cast<U16Type>(v >> 10) + detail::high_surrogate_base;
         m_values[1] = static_cast<U16Type>(v & detail::ten_bit_mask) + detail::low_surrogate_base;
         m_current = 0;
         BOOST_ASSERT(detail::is_high_surrogate(m_values[0]));
         BOOST_ASSERT(detail::is_low_surrogate(m_values[1]));
      }
      else
      {
         // 16-bit code point:
         m_values[0] = static_cast<U16Type>(*m_position);
         m_values[1] = 0;
         m_current = 0;
         // value must not be a surrogate:
         if(detail::is_surrogate(m_values[0]))
            invalid_code_point(*m_position);
      }
   }
   BaseIterator m_position;
   mutable U16Type m_values[3];
   mutable unsigned m_current;
};

template <class BaseIterator, class U32Type = ::boost::uint32_t>
class u16_to_u32_iterator
   : public boost::iterator_facade<u16_to_u32_iterator<BaseIterator, U32Type>, U32Type, std::bidirectional_iterator_tag, const U32Type>
{
   typedef boost::iterator_facade<u16_to_u32_iterator<BaseIterator, U32Type>, U32Type, std::bidirectional_iterator_tag, const U32Type> base_type;
   // special values for pending iterator reads:
   BOOST_STATIC_CONSTANT(::boost::uint32_t, pending_read = 0xffffffffu);

#if !defined(BOOST_NO_STD_ITERATOR_TRAITS) && !defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION)
   typedef typename std::iterator_traits<BaseIterator>::value_type base_value_type;

   BOOST_STATIC_ASSERT(sizeof(base_value_type)*CHAR_BIT == 16);
   BOOST_STATIC_ASSERT(sizeof(U32Type)*CHAR_BIT == 32);
#endif

public:
   typename base_type::reference
      dereference()const
   {
      if(m_value == pending_read)
         extract_current();
      return m_value;
   }
   bool equal(const u16_to_u32_iterator& that)const
   {
      return m_position == that.m_position;
   }
   void increment()
   {
      // skip high surrogate first if there is one:
      if(detail::is_high_surrogate(*m_position)) ++m_position;
      ++m_position;
      m_value = pending_read;
   }
   void decrement()
   {
      --m_position;
      // if we have a low surrogate then go back one more:
      if(detail::is_low_surrogate(*m_position)) 
         --m_position;
      m_value = pending_read;
   }
   BaseIterator base()const
   {
      return m_position;
   }
   // construct:
   u16_to_u32_iterator() : m_position()
   {
   }
   u16_to_u32_iterator(BaseIterator b) : m_position(b)
   {
      m_value = pending_read;
   }
private:
   static void invalid_code_point(::boost::uint16_t val)
   {
#ifndef BOOST_NO_STD_LOCALE
      std::stringstream ss;
      ss << "Misplaced UTF-16 surrogate U+" << std::showbase << std::hex << val << " encountered while trying to encode UTF-32 sequence";
      std::out_of_range e(ss.str());
#else
      std::out_of_range e("Misplaced UTF-16 surrogate encountered while trying to encode UTF-32 sequence");
#endif
      boost::throw_exception(e);
   }
   void extract_current()const
   {
      m_value = static_cast<U32Type>(static_cast< ::boost::uint16_t>(*m_position));
      // if the last value is a high surrogate then adjust m_position and m_value as needed:
      if(detail::is_high_surrogate(*m_position))
      {
         // precondition; next value must have be a low-surrogate:
         BaseIterator next(m_position);
         ::boost::uint16_t t = *++next;
         if((t & 0xFC00u) != 0xDC00u)
            invalid_code_point(t);
         m_value = (m_value - detail::high_surrogate_base) << 10;
         m_value |= (static_cast<U32Type>(static_cast< ::boost::uint16_t>(t)) & detail::ten_bit_mask);
      }
      // postcondition; result must not be a surrogate:
      if(detail::is_surrogate(m_value))
         invalid_code_point(static_cast< ::boost::uint16_t>(m_value));
   }
   BaseIterator m_position;
   mutable U32Type m_value;
};

template <class BaseIterator, class U8Type = ::boost::uint8_t>
class u32_to_u8_iterator
   : public boost::iterator_facade<u32_to_u8_iterator<BaseIterator, U8Type>, U8Type, std::bidirectional_iterator_tag, const U8Type>
{
   typedef boost::iterator_facade<u32_to_u8_iterator<BaseIterator, U8Type>, U8Type, std::bidirectional_iterator_tag, const U8Type> base_type;
   
#if !defined(BOOST_NO_STD_ITERATOR_TRAITS) && !defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION)
   typedef typename std::iterator_traits<BaseIterator>::value_type base_value_type;

   BOOST_STATIC_ASSERT(sizeof(base_value_type)*CHAR_BIT == 32);
   BOOST_STATIC_ASSERT(sizeof(U8Type)*CHAR_BIT == 8);
#endif

public:
   typename base_type::reference
      dereference()const
   {
      if(m_current == 4)
         extract_current();
      return m_values[m_current];
   }
   bool equal(const u32_to_u8_iterator& that)const
   {
      if(m_position == that.m_position)
      {
         // either the m_current's must be equal, or one must be 0 and 
         // the other 4: which means neither must have bits 1 or 2 set:
         return (m_current == that.m_current)
            || (((m_current | that.m_current) & 3) == 0);
      }
      return false;
   }
   void increment()
   {
      // if we have a pending read then read now, so that we know whether
      // to skip a position, or move to a low-surrogate:
      if(m_current == 4)
      {
         // pending read:
         extract_current();
      }
      // move to the next surrogate position:
      ++m_current;
      // if we've reached the end skip a position:
      if(m_values[m_current] == 0)
      {
         m_current = 4;
         ++m_position;
      }
   }
   void decrement()
   {
      if((m_current & 3) == 0)
      {
         --m_position;
         extract_current();
         m_current = 3;
         while(m_current && (m_values[m_current] == 0))
            --m_current;
      }
      else
         --m_current;
   }
   BaseIterator base()const
   {
      return m_position;
   }
   // construct:
   u32_to_u8_iterator() : m_position(), m_current(0)
   {
      m_values[4] = 0;
   }
   u32_to_u8_iterator(BaseIterator b) : m_position(b), m_current(4)
   {
      m_values[4] = 0;
   }
private:
   static void invalid_code_point(::boost::uint32_t val)
   {
#ifndef BOOST_NO_STD_LOCALE
      std::stringstream ss;
      ss << "Invalid UTF-32 code point U+" << std::showbase << std::hex << val << " encountered while trying to encode UTF-8 sequence";
      std::out_of_range e(ss.str());
#else
      std::out_of_range e("Invalid UTF-32 code point encountered while trying to encode UTF-8 sequence");
#endif
      boost::throw_exception(e);
   }

   void extract_current()const
   {
      boost::uint32_t c = *m_position;
      if(c > 0x10FFFFu)
         invalid_code_point(c);
      if(c < 0x80u)
      {
         m_values[0] = static_cast<unsigned char>(c);
         m_values[1] = static_cast<unsigned char>(0u);
         m_values[2] = static_cast<unsigned char>(0u);
         m_values[3] = static_cast<unsigned char>(0u);
      }
      else if(c < 0x800u)
      {
         m_values[0] = static_cast<unsigned char>(0xC0u + (c >> 6));
         m_values[1] = static_cast<unsigned char>(0x80u + (c & 0x3Fu));
         m_values[2] = static_cast<unsigned char>(0u);
         m_values[3] = static_cast<unsigned char>(0u);
      }
      else if(c < 0x10000u)
      {
         m_values[0] = static_cast<unsigned char>(0xE0u + (c >> 12));
         m_values[1] = static_cast<unsigned char>(0x80u + ((c >> 6) & 0x3Fu));
         m_values[2] = static_cast<unsigned char>(0x80u + (c & 0x3Fu));
         m_values[3] = static_cast<unsigned char>(0u);
      }
      else
      {
         m_values[0] = static_cast<unsigned char>(0xF0u + (c >> 18));
         m_values[1] = static_cast<unsigned char>(0x80u + ((c >> 12) & 0x3Fu));
         m_values[2] = static_cast<unsigned char>(0x80u + ((c >> 6) & 0x3Fu));
         m_values[3] = static_cast<unsigned char>(0x80u + (c & 0x3Fu));
      }
      m_current= 0;
   }
   BaseIterator m_position;
   mutable U8Type m_values[5];
   mutable unsigned m_current;
};

template <class BaseIterator, class U32Type = ::boost::uint32_t>
class u8_to_u32_iterator
   : public boost::iterator_facade<u8_to_u32_iterator<BaseIterator, U32Type>, U32Type, std::bidirectional_iterator_tag, const U32Type>
{
   typedef boost::iterator_facade<u8_to_u32_iterator<BaseIterator, U32Type>, U32Type, std::bidirectional_iterator_tag, const U32Type> base_type;
   // special values for pending iterator reads:
   BOOST_STATIC_CONSTANT(::boost::uint32_t, pending_read = 0xffffffffu);

#if !defined(BOOST_NO_STD_ITERATOR_TRAITS) && !defined(BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION)
   typedef typename std::iterator_traits<BaseIterator>::value_type base_value_type;

   BOOST_STATIC_ASSERT(sizeof(base_value_type)*CHAR_BIT == 8);
   BOOST_STATIC_ASSERT(sizeof(U32Type)*CHAR_BIT == 32);
#endif

public:
   typename base_type::reference
      dereference()const
   {
      if(m_value == pending_read)
         extract_current();
      return m_value;
   }
   bool equal(const u8_to_u32_iterator& that)const
   {
      return m_position == that.m_position;
   }
   void increment()
   {
      // skip high surrogate first if there is one:
      unsigned c = detail::utf8_byte_count(*m_position);
      std::advance(m_position, c);
      m_value = pending_read;
   }
   void decrement()
   {
      // Keep backtracking until we don't have a trailing character:
      unsigned count = 0;
      while((*--m_position & 0xC0u) == 0x80u) ++count;
      // now check that the sequence was valid:
      if(count != detail::utf8_trailing_byte_count(*m_position))
         invalid_sequnce();
      m_value = pending_read;
   }
   BaseIterator base()const
   {
      return m_position;
   }
   // construct:
   u8_to_u32_iterator() : m_position()
   {
      m_value = pending_read;
   }
   u8_to_u32_iterator(BaseIterator b) : m_position(b)
   {
      m_value = pending_read;
   }
private:
   static void invalid_sequnce()
   {
      std::out_of_range e("Invalid UTF-8 sequence encountered while trying to encode UTF-32 character");
      boost::throw_exception(e);
   }
   void extract_current()const
   {
      m_value = static_cast<U32Type>(static_cast< ::boost::uint8_t>(*m_position));
      // we must not have a continuation character:
      if((m_value & 0xC0u) == 0x80u)
         invalid_sequnce();
      // see how many extra byts we have:
      unsigned extra = detail::utf8_trailing_byte_count(*m_position);
      // extract the extra bits, 6 from each extra byte:
      BaseIterator next(m_position);
      for(unsigned c = 0; c < extra; ++c)
      {
         ++next;
         m_value <<= 6;
         m_value += static_cast<boost::uint8_t>(*next) & 0x3Fu;
      }
      // we now need to remove a few of the leftmost bits, but how many depends
      // upon how many extra bytes we've extracted:
      static const boost::uint32_t masks[] = 
      {
         0x7Fu,
         0x7FFu,
         0xFFFFu,
         0x1FFFFFu,
      };
      m_value &= masks[extra];
      // check the result:
      if(m_value > 0x10FFFFu)
         invalid_sequnce();
   }
   BaseIterator m_position;
   mutable U32Type m_value;
};

} // namespace boost

#endif // BOOST_REGEX_UNICODE_ITERATOR_HPP