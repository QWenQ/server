/*****************************************************************************

Copyright (c) 1994, 2016, Oracle and/or its affiliates. All Rights Reserved.
Copyright (c) 2019, 2021, MariaDB Corporation.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA

*****************************************************************************/

/******************************************************************//**
@file include/ut0rnd.h
Random numbers and hashing

Created 1/20/1994 Heikki Tuuri
***********************************************************************/

#pragma once

#include "ut0byte.h"
#include <my_sys.h>

/** Seed value of ut_rnd_gen() */
extern std::atomic<uint32_t> ut_rnd_current;

/** @return a pseudo-random 32-bit number */
inline uint32_t ut_rnd_gen()
{
  /* This is a Galois linear-feedback shift register.
  https://en.wikipedia.org/wiki/Linear-feedback_shift_register#Galois_LFSRs
  The generating primitive Galois Field polynomial is the Castagnoli
  polynomial that was made popular by CRC-32C:
  x^32+x^28+x^27+x^26+x^25+x^23+x^22+x^20+
  x^19+x^18+x^14+x^13+x^11+x^10+x^9+x^8+x^6+1 */
  const uint32_t crc32c= 0x1edc6f41;

  uint32_t rnd= ut_rnd_current.load(std::memory_order_relaxed);

  if (UNIV_UNLIKELY(rnd == 0))
  {
    rnd= static_cast<uint32_t>(my_interval_timer());
    if (!rnd) rnd= 1;
  }
  else
  {
    bool lsb= rnd & 1;
    rnd>>= 1;
    if (lsb)
      rnd^= crc32c;
  }

  ut_rnd_current.store(rnd, std::memory_order_relaxed);
  return rnd;
}

/** @return a random number between 0 and n-1, inclusive */
inline ulint ut_rnd_interval(ulint n)
{
  return n > 1 ? static_cast<ulint>(ut_rnd_gen() % n) : 0;
}

# if SIZEOF_SIZE_T < 8
inline size_t ut_fold_ull(uint64_t d) noexcept
{
  return size_t(d) * 31 + size_t(d >> (SIZEOF_SIZE_T * CHAR_BIT));
}
# else
#  define ut_fold_ull(d) d
# endif
/***********************************************************//**
Looks for a prime number slightly greater than the given argument.
The prime is chosen so that it is not near any power of 2.
@return prime */
ulint
ut_find_prime(
/*==========*/
	ulint	n)	/*!< in: positive number > 100 */
	MY_ATTRIBUTE((const));
