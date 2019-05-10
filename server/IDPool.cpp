////////////////////////////////////////////////////////////
//
// RedRelay - a Lacewing Relay protocol reimplementation
// Copyright (c) 2019 LekKit (LekKit#4400 in Discord)
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//   claim that you wrote the original software. If you use this software
//   in a product, an acknowledgment in the product documentation would be
//   appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//   misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#ifndef REDRELAY_ID_POOL
#define REDRELAY_ID_POOL

#include <vector>
#include <iostream>

namespace rs{

IndexedPool::~IndexedPool(){
	Clear();
}

T& IndexedPool::operator[](unsigned int index){
	if (index>=m_pool.size() || m_pool.at(index)==NULL){ //If requested element doesn't exist, returns empty type to prevent segmentation fault
		std::cout<<"SEGFAULT prevented in pool "<<this<<" at index "<<index<<", execution continues"<<std::endl;
		std::cout<<"Please report this incident to LekKit#4400 in Discord! (this is important, yea)"<<std::endl;
		emptytype=T();
		return emptytype;
	}
	return *m_pool.at(index);
}

T* IndexedPool::at(unsigned int index){
	if (AutoScale) while (index>=m_pool.size()) m_pool.push_back(NULL);
	return m_pool.at(index);
}

std::size_t IndexedPool::IndexedPool::Size() const {
	return m_allocated.size();
}

std::vector<IndexedElement<T>>& IndexedPool::GetAllocated(){ //Get all our allocated elements
	return m_allocated;
}

bool IndexedPool::Allocated(unsigned int index) const { //Check if specified ID is busy
	return !(index>=m_pool.size() || m_pool.at(index)==NULL);
}

void IndexedPool::Allocate(unsigned int index){ //Allocate element with given ID
	if (AutoScale) while (index>=m_pool.size()) m_pool.push_back(NULL);
	if (m_pool.at(index)==NULL){ //Only if our ID isn't busy already
		m_pool.at(index)=new T;
		IndexedElement<T> IndexedElement;
		IndexedElement.index = index;
		IndexedElement.element = m_pool.at(index);
		m_allocated.push_back(IndexedElement);
	}
}

void IndexedPool::Deallocate(unsigned int index){ //Destroy element at given ID
	if (index<m_pool.size() && m_pool.at(index)!=NULL){ //Only if this element exists
		for (unsigned int i=0; i<m_allocated.size(); ++i) if (m_allocated.at(i).index==index) m_allocated.erase(m_allocated.begin()+i);
		delete m_pool.at(index);
		m_pool.at(index) = NULL;
	}
}

void IndexedPool::Clear(){
	m_pool.clear();
	m_pool.shrink_to_fit();
	for (uint32_t i=0; i<m_allocated.size(); ++i) delete m_allocated.at(i).element;
	m_allocated.clear();
	m_allocated.shrink_to_fit();
}
	
}

#endif
