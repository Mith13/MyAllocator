
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>
#include <iostream>
#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include <stdlib.h>
class MyAllocator {
private:
	/* memory segment header structure */
	class segment {
	public:
		// true if this segment is used (allocated) 
		bool used;
		// size of the memory block allocated for data
		size_t size;
		// pointers to previous and next segments 
		segment * prev, *next;
	};
	// allocated pools of memory, new one is allocated when
	// there is no space in previous. 
	std::vector<segment*> pool;
    
	// initialize first segment of pool
	inline segment* initialize(size_t size)
	{
		std::cout << "Want to allocated this much: " << size << std::endl;
		size_t size_to_alloc = size;
		if (size == 0) {
			std::bad_alloc zero_size_exception;
			throw zero_size_exception;
		}
		//if ((int)size < 1048576) {
		if (size < 201) {
			size_to_alloc = 200;
		}
		segment* first_segment;

		// initialize memory for this pool
		char* mem = new char[size_to_alloc];
		first_segment = reinterpret_cast<segment *> (mem);

		// mark it unused
		first_segment->used = false;

		// the size is the size of allocated memory minus the segment header
		first_segment->size = size_to_alloc - sizeof(segment);

		// no previous or next segments
		first_segment->prev = nullptr;
		first_segment->next = nullptr;
		//push the pointer to pool to pool container
		pool.push_back(first_segment);
		std::cout << "Allocated new pool with size of " << size_to_alloc << " at " << (void *)mem << "|" << (void *)first_segment << "[" << (void *)pool[0] << "]" << std::endl;
		return first_segment;
	}

	// given a segment header returns the pointer to the segment data
	inline void* segment_data(segment * seg)
	{
		return (reinterpret_cast<char *> (seg)) + sizeof(segment);
	}

	// given a pointer to the segment data returns the segment header 
	inline segment *segment_header(void * data)
	{
		return reinterpret_cast<segment *> (static_cast<char *> (data) - sizeof(segment));
	}

	/* converts this:
							|
	   +-----+--------------+--------------------------------------------+
	   | hdr |                         old size                          |
	   +-----+--------------+--------------------------------------------+
							|
	   to this:             |
							|
	   +-----+--------------+-----+--------------------------------------+
	   | hdr |   new_size   | hdr |      size of new unused segment      |
	   +-----+--------------+-----+--------------------------------------+
	   */
	inline void segment_shrink(segment * to_shrink, size_t size)
	{
		// get the new segment header 
		segment * seg = reinterpret_cast<segment *> (reinterpret_cast<char *> (to_shrink) + sizeof(segment) + size);

		// mark it unused and compute its size 
		seg->used = false;
		seg->size = to_shrink->size - sizeof(segment) - size;

		// set the pointers to the previous and next segments on all concerned segments 
		seg->prev = to_shrink;
		seg->next = to_shrink->next;

		to_shrink->next = seg;

		if (seg->next != nullptr) {
			seg->next->prev = seg;
		}

		// set the size to new_size
		to_shrink->size = size;
	}
public:

	/* Constructor */
	MyAllocator() {
		std::cout << "Segment header size is " << sizeof(segment) << std::endl;
		//pool.reserve(1);
	}
	~MyAllocator() {
        std::cout<<std::endl;
		for (int i = 0; i < pool.size(); i++) {
			std::cout << "Freeing " << (void *)pool[i] << std::endl;
			delete[](reinterpret_cast<char *> (pool[i]));
		}

	}
	// tries to allocate a memory of size bytes, throws error if it fails
	void* allocate(size_t size)
	{
		segment* seg = nullptr;
		void * result;
		if (pool.size() == 0) {
			try {
				initialize(size);
			}
			catch (std::bad_alloc&) {
				std::cerr << "Allocating zero bytes" << std::endl;
				throw;
			}
		}
		bool can_allocate = false;
		for (auto first_segment = pool.begin(); first_segment != pool.end(); first_segment++) {
			// find first free segment with enough space 
			for (seg = *first_segment; seg != nullptr; seg = seg->next) {
				if (!seg->used && seg->size >= size) {
					can_allocate = true;
					break;
				}
			}
			if (can_allocate)break;
		}
		if (!can_allocate) {
			try {
				seg = initialize(size);
			}
			catch (std::bad_alloc&) {
				std::cerr << "Allocating zero bytes" << std::endl;
				this->~MyAllocator();
				throw;
			}
			catch (...) {
				this->~MyAllocator();
				throw;
			}

		}

		if (seg != nullptr) {
			// free segment was found, mark it used 
			seg->used = true;
			result = segment_data(seg);

			if (seg->size > size + sizeof(segment)) {
				/* this segment is big enough to store requested size
				   and another segment as well (with at least one byte
				   for data allocation) */
				segment_shrink(seg, size);
			}
		}
		else {
			std::bad_alloc ba;
			std::cout << "Cannot allocate" << std::endl;
			throw ba;
		}

		return result;
	}

	/* frees memory pointed to by ptr allocated by previous call to allocate */
	void deallocate(void * ptr)
	{
		
		std::cout << "Deleting allocated ptr " << ptr << std::endl;
		segment * seg = segment_header(ptr);
		if (seg->size == 0) {
			std::bad_alloc ba;
			std::cout << "Wrong pointer";
			throw ba;
		}
		seg->used = false;
		if (seg->next != nullptr && !seg->next->used) {
			segment* next = seg->next;
			seg->size = seg->size + sizeof(segment) + next->size;
			if (next->next != nullptr) {
				next->next->prev = seg;
				seg->next = next->next;
			}
			else seg->next = nullptr;
		}
		if (seg->prev != nullptr && !seg->prev->used) {
			segment* prev = seg->prev;
			prev->size = prev->size + sizeof(segment) + seg->size;
			if (seg->next != nullptr) seg->next->prev = prev;
			prev->next = seg->next;
		}
		
	}
	void printMemoryLayout()
	{
		std::cout << "\nPrinting allocated memory layout\n";
		for (auto first_segment = pool.begin(); first_segment != pool.end(); first_segment++) {
			std::cout <<"Pool:"<< std::endl;
			segment* seg;
			seg = *first_segment;
			if (seg->used)std::cout << "| hdr |   " << seg->size << "   ";
			else std::cout << "| hdr |  &" << seg->size << "&  ";
			for (seg = (*first_segment)->next; seg != nullptr; seg = seg->next) {
				if (seg->used)std::cout << "| hdr |   " << seg->size << "   ";
				else std::cout << "| hdr |  &" << seg->size << "&  ";
			}
			std::cout << "|" << std::endl;
		}
	}
};
int main(int argc, char** argv) {

	MyAllocator alloc;
	double* a = new (alloc.allocate(sizeof(double))) double(2);
	alloc.printMemoryLayout();

	double* aa = new (alloc.allocate(sizeof(double))) double(2);
	alloc.printMemoryLayout();
	char* b = new(alloc.allocate(1000)) char[1000];
	alloc.printMemoryLayout();
	alloc.deallocate(aa);

	b[300] = 'c';
	std::cout <<"CHhar "<<b[300];
	alloc.printMemoryLayout();
	std::cout << "size of doube" << sizeof(double);

}
