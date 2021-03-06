//-------------------------------------------------------------------------------------------------
#if 0

Fix8 is released under the GNU LESSER GENERAL PUBLIC LICENSE Version 3.

Fix8 Open Source FIX Engine.
Copyright (C) 2010-13 David L. Dight <fix@fix8.org>

Fix8 is free software: you can  redistribute it and / or modify  it under the  terms of the
GNU Lesser General  Public License as  published  by the Free  Software Foundation,  either
version 3 of the License, or (at your option) any later version.

Fix8 is distributed in the hope  that it will be useful, but WITHOUT ANY WARRANTY;  without
even the  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

You should  have received a copy of the GNU Lesser General Public  License along with Fix8.
If not, see <http://www.gnu.org/licenses/>.

THE EXTENT  PERMITTED  BY  APPLICABLE  LAW.  EXCEPT WHEN  OTHERWISE  STATED IN  WRITING THE
COPYRIGHT HOLDERS AND/OR OTHER PARTIES  PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY
KIND,  EITHER EXPRESSED   OR   IMPLIED,  INCLUDING,  BUT   NOT  LIMITED   TO,  THE  IMPLIED
WARRANTIES  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS TO
THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU. SHOULD THE PROGRAM PROVE DEFECTIVE,
YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION.

IN NO EVENT UNLESS REQUIRED  BY APPLICABLE LAW  OR AGREED TO IN  WRITING WILL ANY COPYRIGHT
HOLDER, OR  ANY OTHER PARTY  WHO MAY MODIFY  AND/OR REDISTRIBUTE  THE PROGRAM AS  PERMITTED
ABOVE,  BE  LIABLE  TO  YOU  FOR  DAMAGES,  INCLUDING  ANY  GENERAL, SPECIAL, INCIDENTAL OR
CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT
NOT LIMITED TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH
HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

#endif
//-------------------------------------------------------------------------------------------------
#ifndef _FIX8_FF_WRAPPER_HPP_
#define _FIX8_FF_WRAPPER_HPP_

//-------------------------------------------------------------------------------------------------
namespace FIX8 {

//----------------------------------------------------------------------------------------
template<typename T>
class ff_unbounded_queue
{
	// we could also use ff::MSqueue
	ff::uMPMC_Ptr_Queue _queue;

public:
	typedef T value_type;

	//! Reference type
	typedef T& reference;

	//! Const reference type
	typedef const T& const_reference;

	explicit ff_unbounded_queue() { _queue.init(); }
	~ff_unbounded_queue() {}

	bool try_push(const T& source)
		{ return _queue.push(new (::ff::ff_malloc(sizeof(T))) T(source)); }

	void push(const T& source) { try_push(source); }
	bool try_pop(T* &target) { return _queue.pop(reinterpret_cast<void**>(&target)); }
	bool pop(T* &target, const unsigned ns=0)
	{
		for(;;)
		{
			if (try_pop(target))
				return true;
			if (ns == 0)
				break;
			hypersleep<h_nanoseconds>(ns);
		}
		return false;
	}
	void release(T *source) const { ::ff::ff_free(source); }
};

//----------------------------------------------------------------------------------------
template<typename T> // pointer specialisation - treat the pointer version identically and gobble the indirection
class ff_unbounded_queue<T*>
{
	ff::uMPMC_Ptr_Queue _queue;

public:
	typedef T value_type;

	//! Reference type
	typedef T &reference;

	//! Const reference type
	typedef const T &const_reference;

	explicit ff_unbounded_queue() { _queue.init(); }
	~ff_unbounded_queue() {}

	bool try_push(T *source) { return _queue.push(source); }
	void push(T *source) { try_push(source); }
	bool try_pop(T* &target) { return _queue.pop(reinterpret_cast<void**>(&target)); }
	bool pop(T* &target, const unsigned ns=0)
	{
		for(;;)
		{
			if (try_pop(target))
				return true;
			if (ns == 0)
				break;
			hypersleep<h_nanoseconds>(ns);
		}
		return false;
	}
};

//----------------------------------------------------------------------------------------
// C++ wrapper around fastflow atomic
template<typename T>
class ff_atomic
{
	mutable atomic_long_t _rep;

public:
	typedef T value_type;

	value_type operator=(const value_type rhs)
		{ atomic_long_set(&_rep, rhs); return rhs; }

	ff_atomic<value_type>& operator=(const ff_atomic<value_type>& rhs)
		{ atomic_long_set(&_rep, atomic_long_read(&rhs._rep)); return *this; }

	value_type operator++(int) // postfix
		{ value_type v = atomic_long_read(&_rep); atomic_long_inc(&_rep); return v; }
	value_type operator--(int) // postfix
		{ value_type v = atomic_long_read(&_rep); atomic_long_dec(&_rep); return v; }
	value_type operator++() // prefix
		{ return atomic_long_inc_return(&_rep); }
	value_type operator--() // prefix
		{ return atomic_long_dec_return(&_rep); }
	value_type operator+=(value_type value)
		{ atomic_long_add(value, &_rep); return atomic_long_read(&_rep); }
	value_type operator-=(value_type value)
		{ atomic_long_sub(value, &_rep); return atomic_long_read(&_rep); }

	operator value_type() { return static_cast<T>(atomic_long_read(&_rep)); }
	operator value_type() const { return static_cast<T>(atomic_long_read(&_rep)); }
};

//----------------------------------------------------------------------------------------
// pointer specialisation of above
template<typename T>
class ff_atomic<T*>
{
	atomic_long_t _rep;

public:
	typedef T* value_type;

	T* operator=(T* rhs)
	{
		atomic_long_set(&_rep, reinterpret_cast<long>(rhs));
		return reinterpret_cast<T*>(atomic_long_read(&_rep));
	}

	ff_atomic<T*>& operator=(const ff_atomic<T*>& rhs)
		{ atomic_long_set(&_rep, atomic_long_read(rhs._rep)); return *this; }

	T* operator->() const { return reinterpret_cast<T*>(atomic_long_read(&_rep)); }
	T operator*() { return *reinterpret_cast<T*>(atomic_long_read(&_rep)); }

	operator value_type() { return reinterpret_cast<T*>(atomic_long_read(&_rep)); }
	operator value_type() const { return reinterpret_cast<T*>(atomic_long_read(&_rep)); }
};

//----------------------------------------------------------------------------------------
// generic pthread_mutex wrapper
class f8_mutex
{
	pthread_mutex_t pmutex;

public:
	f8_mutex()
	{
		if (pthread_mutex_init(&pmutex, 0))
			throw f8Exception("pthread_mutex_init failed");
	}

	~f8_mutex() { pthread_mutex_destroy(&pmutex); };

	void lock() { pthread_mutex_lock(&pmutex); }
	bool try_lock() { return pthread_mutex_trylock(&pmutex) == 0; }
	void unlock() { pthread_mutex_unlock(&pmutex); }

	friend class f8_scoped_lock;
};

//----------------------------------------------------------------------------------------
class f8_scoped_lock
{
	f8_mutex *local_mutex;

	f8_scoped_lock(const f8_scoped_lock&);
	f8_scoped_lock& operator=(const f8_scoped_lock&);

public:
	f8_scoped_lock() : local_mutex() {}
	f8_scoped_lock(f8_mutex& f8_mutex) { acquire(f8_mutex); }
	~f8_scoped_lock()
	{
		if (local_mutex)
			release();
	}

	void acquire(f8_mutex& f8_mutex)
	{
		f8_mutex.lock();
		local_mutex = &f8_mutex;
	}

	bool try_acquire(f8_mutex& f8_mutex)
	{
		bool result(f8_mutex.try_lock());
		if(result)
			local_mutex = &f8_mutex;
		return result;
	}

	void release()
	{
		local_mutex->unlock();
		local_mutex = 0;
	}

	friend class f8_mutex;
};

//----------------------------------------------------------------------------------------

} // FIX8

#endif // _FIX8_FF_WRAPPER_HPP_

