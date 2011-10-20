/*
 * Copyright (C) 2004-2007 Marc Boris Duerner
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * As a special exception, you may use this file as part of a free
 * software library without restriction. Specifically, if other files
 * instantiate templates or use macros or inline functions from this
 * file, or you compile this file and link it with other files to
 * produce an executable, this file does not by itself cause the
 * resulting executable to be covered by the GNU General Public
 * License. This exception does not however invalidate any other
 * reasons why the executable file might be covered by the GNU Library
 * General Public License.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <cxxtools/string.h>
//#include <cxxtools/log.h>

//log_define("cxxtools.string")
#define log_debug(m)

namespace std
{

basic_string<cxxtools::Char>::basic_string(const basic_string& str)
: _data(0)
{
    // if the other string is not being modified with iterators we can
    // share the same data. Otherwise the other string is marked as
    // busy and we need to copy on write
    if( str._data->busy() == false ) {
        _data = str._data;
        _data->ref();
        log_debug(static_cast<void*>(this) << " copy not busy (\"" << narrow() << "\")");
    }
    else {
        _data = new cxxtools::StringData( str._data->str(), str._data->length() );
        log_debug("copy busy (\"" << narrow() << "\")");
    }
}


basic_string<cxxtools::Char>::~basic_string()
{
    // Noone else references this data if the reference counter
    // is at one or this string is marked as busy because someone
    // has a mutating iterator on it.
    if( _data->busy() || _data->unref() < 1 ) {
        log_debug(static_cast<void*>(this) << " delete data (\"" << narrow() << "\")");
        delete _data;
        _data = 0;
    }
    else
    {
        log_debug(static_cast<void*>(this) << " unrefed");
    }
}


basic_string<cxxtools::Char>::iterator basic_string<cxxtools::Char>::begin()
{
    this->detach( _data->length() );
    _data->setBusy();
    return _data->str();
}


basic_string<cxxtools::Char>::iterator basic_string<cxxtools::Char>::end()
{
    this->detach( _data->length() );
    _data->setBusy();
    return _data->end();
}


void basic_string<cxxtools::Char>::resize(size_t n, cxxtools::Char ch)
{
    size_type size = this->size();
    if(size < n) {
        this->append(n - size, ch);
    }
    else if(n < size) {
        this->erase(n);
    }

    // do nothing if n == size

    // mutation ends busy mode
    _data->setInitial();
}


void basic_string<cxxtools::Char>::reserve(size_t n)
{
    if( (n == this->capacity() && _data->busy()) || n == 0 )
        return;

    const size_type size = this->size();

    if(n > size)
        this->detach(n);
    else if(size > n)
        this->detach(size);

    // mutation ends busy mode
    _data->setInitial();
}


void basic_string<cxxtools::Char>::detach(size_type reserveSize)
{
    // shared, not busy - make copy
    if( _data->shared() ) 
    {
        cxxtools::StringData* newBuffer = new cxxtools::StringData();
        newBuffer->reserve( reserveSize );
        newBuffer->assign( _data->str(), _data->length() );

        if( _data->unref() < 1)
        {
            // just in case two threads are trying this at once
            delete newBuffer;
        }
        else
        {
            _data = newBuffer;
        }
    }
    // just resizing
    else
    {
        _data->reserve( reserveSize );
    }
}


void basic_string<cxxtools::Char>::clear()
{
    if (!empty() )
    {
        this->detach(0);
        _data->setInitial();
        _data->clear();
    }
}


void basic_string<cxxtools::Char>::swap(basic_string& str)
{
    // mutation ends busy states
    if( _data->busy() )
        _data->setInitial();

    if( str._data->busy() )
        str._data->setInitial();

    // swap the data pointers
    cxxtools::StringData* tmp = this->_data;
    this->_data = str._data;
    str._data = tmp;

    // TODO: handle different allocator types when this class is
    // templated for allocator types. We need to deep copy then
    //
    // basic_string tmp1(this->c_str(), this->length(), this->get_allocator());
    // basic_string tmp2(str.c_str(), str.length(), str.get_allocator());
    // str = tmp1;
    // *this = tmp2;
}



basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::copy(cxxtools::Char* a, size_type n, size_type pos) const
{
    if( pos > this->size() ) {
        throw out_of_range("basic_string::copy");
    }

    if(n > this->size() - pos) {
        n = this->size() - pos;
    }

    traits_type::copy(a, _data->str() + pos, n);
    return n;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::assign(const basic_string<cxxtools::Char>& str)
{
    // self-assignment check
    if(this == &str)
    {
        log_debug("self assign (\"" << str.narrow() << "\")");
        return *this;
    }

    if( _data->shared() )
    {
        log_debug(static_cast<void*>(this) << " shared assign, busy=" << str._data->busy() << " (\"" << str.narrow() << "\")");
        cxxtools::StringData* newBuffer = str._data;
        if( str._data->busy() )
        {
            newBuffer = new cxxtools::StringData( str._data->str(), str._data->length() );
        }
        else
        {
            newBuffer->ref();
        }

        if( _data->unref() < 1)
        {
            // just in case two threads are trying this at once
            delete _data;
        }

        _data = newBuffer;
    }
    else // unshared
    {
        log_debug("unshared assign (\"" << str.narrow() << "\")");
        if( _data->capacity() >= str.size() || str._data->busy() )
        {
            _data->assign( str._data->str(), str._data->length() );
            _data->setInitial();
        }
        else
        {
            delete _data;
            _data = str._data;
            _data->ref();
        }
    }

    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::assign(const cxxtools::Char* str)
{
    // self-assignment check
    if(_data->str() != str)
    {
        const size_type len = char_traits<cxxtools::Char>::length(str);
        this->assign(str, len);
    }

    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::assign(const cxxtools::Char* str, size_type length)
{
    // this is a modifying action and if multiple instances reference this
    // data instance we need to copy on write first.
    if( _data->shared() )
    {
        cxxtools::StringData* newBuffer = new cxxtools::StringData( str, length );
        _data->unref();
        _data = newBuffer;
    }
    else { // unshared or busy, dont copy
        _data->assign(str, length);
        // mutate ends busy mode
        _data->setInitial();
    }

    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::assign(size_type n, cxxtools::Char ch)
{
    // copy if shared and not busy
    if( _data->shared() ) {
        cxxtools::StringData* newBuffer = new cxxtools::StringData( n, ch );
        _data->unref();
        _data = newBuffer;
    }
    else { // unshared or busy, dont copy
        _data->assign(n, ch);
        // mutate ends busy mode
        _data->setInitial();
    }

    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::append(const cxxtools::Char* str)
{
    return this->append( str, traits_type::length(str) );
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::append(const cxxtools::Char* str, size_type n)
{
    // shared, not busy - work on copy
    if( _data->shared() ) {
        cxxtools::StringData* newBuffer = new cxxtools::StringData( _data->str(), _data->length() );
        _data->unref();
        _data = newBuffer; // refs == 1
    }

    // mutation ends busy mode
    _data->setInitial();

    _data->append(str, n);
    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::append(size_type n, cxxtools::Char ch)
{
    // shared, not busy - work on copy
    if( _data->shared() ) {
        cxxtools::StringData* newBuffer = new cxxtools::StringData();
        newBuffer->reserve(_data->length() + n);
        newBuffer->assign( _data->str(), _data->length() );

        _data->unref();
        _data = newBuffer; // refs == 1
    }

    // mutate ends busy mode
    _data->setInitial();
    _data->append(n, ch);

    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::insert(size_type pos, const cxxtools::Char* str, size_type n)
{
    // detach to new size
    this->detach( _data->length() + n );

    // mutation ends busy mode
    _data->setInitial();

    _data->insert(pos, str, n);

    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::insert(size_type pos, size_type n, cxxtools::Char ch)
{
    this->detach( _data->length() + n );

    _data->setInitial();

    _data->insert(pos, n, ch);

    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::insert(size_type pos, const basic_string& str)
{
    this->detach( _data->length() + str.length() );

    _data->setInitial();

    _data->insert(pos, str._data->str(), str._data->length());

    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::insert(size_type pos, const basic_string& str, size_type pos2, size_type n)
{
    this->detach( _data->length() + n );

    _data->setInitial();

    _data->insert(pos, str._data->str() + pos2, n);

    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::insert(iterator p, size_type n, cxxtools::Char ch)
{
    const size_type pos = p - _data->str();

    this->detach( _data->length() + n );

    _data->setInitial();

    _data->insert(pos, n, ch);

    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::erase(size_type pos, size_type n)
{
    this->detach( _data->length() );
    _data->setInitial();

    const size_type len = this->size() - pos;
    if(n > len) {
        n = len;
    }

    _data->erase(_data->str() + pos, n);
    return *this;
}


basic_string<cxxtools::Char>::iterator
basic_string<cxxtools::Char>::erase(iterator it)
{
    const size_type pos = it - _data->str();
    this->detach( _data->length() );
    _data->setInitial();
    return _data->erase(_data->str() + pos, 1);
}


basic_string<cxxtools::Char>::iterator
basic_string<cxxtools::Char>::erase(iterator first, iterator last)
{
    const size_type pos = first - _data->str();
    const size_type n = last - first;
    this->detach( _data->length() );
    _data->setInitial();
    return _data->erase(_data->str() + pos, n);
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::replace(size_type pos, size_type n, const cxxtools::Char* str)
{
    this->detach( _data->length() );
    _data->setInitial();
    _data->replace( pos, n, str, traits_type::length(str) );
    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::replace(size_type pos, size_type n, const cxxtools::Char* str, size_type n2)
{
    this->detach( _data->length() );
    _data->setInitial();
    _data->replace( pos, n, str, n2 );
    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::replace(size_type pos, size_type n, size_type n2, cxxtools::Char ch)
{
    this->detach( _data->length() );
    _data->setInitial();
    _data->replace(pos, n, n2, ch);
    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::replace(size_type pos, size_type n, const basic_string& str)
{
    this->detach( _data->length() );
    _data->setInitial();
    _data->replace( pos, n, str._data->str(), str._data->length() );
    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::replace(size_type pos, size_type n,
                                                        const basic_string& str, size_type pos2, size_type n2)
{
    this->detach( _data->length() );
    _data->setInitial();
    _data->replace( pos, n, str._data->str() + pos2, n2 );
    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::replace(iterator i1, iterator i2, const cxxtools::Char* str)
{
    this->detach( _data->length() );
    _data->setInitial();
    _data->replace( i1 - _data->str(), i2 - i1, str, traits_type::length(str) );
    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::replace(iterator i1, iterator i2, const cxxtools::Char* str, size_type n)
{
    this->detach( _data->length() );
    _data->setInitial();
    _data->replace( i1 - _data->str(), i2 - i1, str, n );
    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::replace(iterator i1, iterator i2, size_type n, cxxtools::Char ch)
{
    this->detach( _data->length() );
    _data->setInitial();
    _data->replace( i1 - _data->str(), i2 - i1, n, ch );
    return *this;
}


basic_string<cxxtools::Char>& basic_string<cxxtools::Char>::replace(iterator i1, iterator i2, const basic_string& str)
{
    this->detach( _data->length() );
    _data->setInitial();
    _data->replace( i1 - _data->str(), i2 - i1, str._data->str(), str.length() );
    return *this;
}


int basic_string<cxxtools::Char>::compare(const basic_string& str) const
{
    const size_type size = this->size();
    const size_type osize = str.size();
    size_type n = min(size , osize);

    const int result = traits_type::compare(_data->str(), str._data->str(), n);

    // unlike real life, size only matters when the quality is equal
    if (result == 0) {
        return static_cast<int>(size - osize);
    }

    return result;
}


int basic_string<cxxtools::Char>::compare(const cxxtools::Char* str) const
{
    const size_type size = this->size();
    const size_type osize = traits_type::length(str);
    size_type n = min(size , osize);

    const int result = traits_type::compare(_data->str(), str, n);

    // unlike real life, size only matters when the quality is equal
    if (result == 0) {
        return static_cast<int>(size - osize);
    }

    return result;
}


int basic_string<cxxtools::Char>::compare(const wchar_t* str) const
{
    const cxxtools::Char* self = _data->str();
    while(*self && *str)
    {
        if( *self != *str )
            return *self < cxxtools::Char(*str) ? -1 : +1;

        ++self;
        ++str;
    }

    return static_cast<int>( *self - cxxtools::Char(*str) );
}


int basic_string<cxxtools::Char>::compare(size_type pos, size_type n, const basic_string& str) const
{
    const size_type size = n;
    const size_type osize = str.size();
    size_type len = min(size , osize);

    const int result = traits_type::compare(_data->str() + pos, str._data->str(), len);

    // unlike real life, size only matters when the quality is equal
    if (result == 0) {
        return static_cast<int>(size - osize);
    }

    return result;
}


int basic_string<cxxtools::Char>::compare(size_type pos, size_type n, const basic_string& str, size_type pos2, size_type n2) const
{
    const size_type size = n;
    const size_type osize = n2;
    size_type len = min(size , osize);

    const int result = traits_type::compare(_data->str() + pos,
                                            str._data->str() + pos2,
                                            len);

    // unlike real life, size only matters when the quality is equal
    if (result == 0) {
        return static_cast<int>(size - osize);
    }

    return result;
}


int basic_string<cxxtools::Char>::compare(size_type pos, size_type n, const cxxtools::Char* str) const
{
    const size_type size = n;
    const size_type osize = traits_type::length(str);
    size_type len = min(size , osize);

    const int result = traits_type::compare(_data->str() + pos,
                                            str,
                                            len);

    // unlike real life, size only matters when the quality is equal
    if (result == 0) {
        return static_cast<int>(size - osize);
    }

    return result;
}


int basic_string<cxxtools::Char>::compare(size_type pos, size_type n, const cxxtools::Char* str, size_type n2) const
{
    const size_type size = n;
    const size_type osize = n2;
    size_type len = min(size , osize);

    const int result = traits_type::compare(_data->str() + pos,
                                            str,
                                            len);

    // unlike real life, size only matters when the quality is equal
    if (result == 0) {
        return static_cast<int>(size - osize);
    }

    return result;
}


basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::find(const basic_string& str, size_type pos) const
{
    return this->find( str.data(), pos, str.size() );
}


basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::find(const cxxtools::Char* token, size_type pos, size_type n) const
{
    const size_type size = this->size();
    const cxxtools::Char* str = _data->str();

    for( ; pos + n <= size; ++pos) {
        if( 0 == traits_type::compare( str + pos, token, n ) ) {
            return pos;
        }
    }

    return npos;
}


basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::find(cxxtools::Char ch, size_type pos) const
{
    const size_type size = this->size();
    if(pos > size) {
        return npos;
    }

    const cxxtools::Char* str = _data->str();
    const size_type n = size - pos;

    const cxxtools::Char* found = traits_type::find(str + pos, n, ch);
    if(found) {
        return found - str;
    }

    return npos;
}


basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::rfind(const basic_string& str, size_type pos) const
{
    return this->rfind( str.data(), pos, str.size() );
}


basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::rfind(const cxxtools::Char* token, size_type pos, size_type n) const
{
    // FIXME: check length
    const size_type size = this->size();

    if (n > size) {
        return npos;
    }

    pos = min(size_type(size - n), pos);
    const cxxtools::Char* str = _data->str();
    do {
        if (traits_type::compare(str + pos, token, n) == 0)
        return pos;
    }
    while (pos-- > 0);

    return npos;
}


basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::rfind(cxxtools::Char ch, size_type pos) const
{
    const cxxtools::Char* str = _data->str();
    size_type size = this->size();

    if(size == 0)
        return npos;

    if(--size > pos)
        size = pos;

    for(++size; size-- > 0; ) {
        if( traits_type::eq(str[size], ch) )
            return size;
    }

    return npos;
}

basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::find_first_of(const cxxtools::Char* s, size_type pos, size_type n) const
{
    // check length os s against n
    const cxxtools::Char* str = _data->str();
    const size_type size = this->size();

    for (; n && pos < size; ++pos) {
        if( traits_type::find(s, n, str[pos]) )
            return pos;
    }

    return npos;
}

basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::find_last_of(const cxxtools::Char* s, size_type pos, size_type n) const
{
    // check length os s against n
    size_type size = this->size();
    const cxxtools::Char* str = _data->str();

    if (size == 0 ||  n == 0) {
        return npos;
    }

    if (--size > pos) {
        size = pos;
    }

    do {
        if( traits_type::find(s, n, str[size]) )
            return size;
    }
    while (size-- != 0);


    return npos;
}

basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::find_first_not_of(const cxxtools::Char* tok, size_type pos, size_type n) const
{
    // pt_requires_string_len(str, n);
    const cxxtools::Char* str = _data->str();

    for (; pos < this->size(); ++pos) {
        if ( !traits_type::find(tok, n, str[pos]) )
            return pos;
    }
    return npos;
}

basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::find_first_not_of(cxxtools::Char ch, size_type pos) const
{
    const cxxtools::Char* str = _data->str();

    for (; pos < this->size(); ++pos) {
        if ( !traits_type::eq(str[pos], ch) ) {
            return pos;
        }
    }

    return npos;
}


basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::find_last_not_of(const cxxtools::Char* tok, size_type pos, size_type n) const
{
    //requires_string_len(__s, __n);
    size_type size = this->size();
    const cxxtools::Char* str = _data->str();

    if(size) {
        if (--size > pos)
            size = pos;

        do {
            if ( !traits_type::find(tok, n, str[size]) ) {
                return size;
            }
        }
        while(size--);
    }

    return npos;
}

basic_string<cxxtools::Char>::size_type
basic_string<cxxtools::Char>::find_last_not_of(cxxtools::Char ch, size_type pos) const
{
    size_type size = this->size();
    const cxxtools::Char* str = _data->str();

    if (size) {
        if (--size > pos)
            size = pos;
        do {
            if( !traits_type::eq(str[size], ch) ) {
                return size;
            }
        } while (size--);
    }

    return npos;
}


std::string basic_string<cxxtools::Char>::narrow(char dfault) const
{
    std::string ret;
    size_type len = this->length();
    const cxxtools::Char* s = _data->str();

    ret.reserve(len);

    for(size_t n = 0; n < len; ++n){
        ret.append( 1, s->narrow(dfault) );
        ++s;
    }

    return ret;
}


basic_string<cxxtools::Char> basic_string<cxxtools::Char>::widen(const std::string& str)
{
    std::basic_string<cxxtools::Char> ret;

    ret.reserve(str.size());

    for(size_t n = 0; n < str.size(); ++n)
        ret += cxxtools::Char( str[n] );

    return ret;
}


}
