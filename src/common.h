#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MIN(a,b) (a < b ? a : b)
#define MAX(a,b) (a > b ? a : b)

#ifdef _MSC_VER
#define INLINE __forceinline
#else
#define INLINE __attribute__((always_inline))
#endif

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

template<typename T>
struct Array {
    T *data = 0;
    s64 length = 0;
    s64 allocated = 0;
    
    const int NEW_MEM_CHUNK_ELEMENT_COUNT =  16;
    
    Array(s64 reserve_amount = 0) {
        reserve(reserve_amount);
    }
    
    ~Array() {
        reset();
    }
    
    void reserve(s64 amount) {
        if (amount <= 0) amount = NEW_MEM_CHUNK_ELEMENT_COUNT;
        if (amount <= allocated) return;
        
        T *new_mem = (T *)malloc(amount * sizeof(T));
        
        if (data) {
            memcpy(new_mem, data, length * sizeof(T));
            free(data);
        }
        
        data = new_mem;
        allocated = amount;
    }
    
    void resize(s64 amount) {
        reserve(amount);
        length = amount;
    }
    
    void add(T element) {
        if (length+1 >= allocated) reserve(allocated * 2);
        
        data[length] = element;
        length += 1;
    }
    
    T unordered_remove(s64 index) {
        assert(index >= 0 && index < length);
        assert(length);
        
        T last = pop();
        if (index < length) {
            (*this)[index] = last;
        }
        
        return last;
    }

    T ordered_remove(s64 index) {
        assert(index >= 0 && index < length);
        assert(length);

        T item = (*this)[index];
        memmove(data + index, data + index + 1, ((length - index) - 1) * sizeof(T));

        length--;
        return item;
    }
    
    T pop() {
        assert(length > 0);
        T result = data[length-1];
        length -= 1;
        return result;
    }
    
    void clear() {
        length = 0;
    }
    
    void reset() {
        length = 0;
        allocated = 0;
        
        if (data) free(data);
        data = 0;
    }
    
    T &operator[] (s64 index) {
        assert(index >= 0 && index < length);
        return data[index];
    }
    
    T *begin() {
        return &data[0];
    }
    
    T *end() {
        return &data[length];
    }
};