// source: https://stackoverflow.com/questions/1225998/what-is-a-bitmap-in-c

#include <limits.h> /* for CHAR_BIT */
#include <stdint.h> /* for uint64_t */

enum
{
    BITS_PER_WORD = sizeof(uint64_t) * CHAR_BIT
};
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b) ((b) % BITS_PER_WORD)

void set_bit(uint64_t *words, int n)
{
    words[WORD_OFFSET(n)] |= (1 << BIT_OFFSET(n));
}

void clear_bit(uint64_t *words, int n)
{
    words[WORD_OFFSET(n)] &= ~(1 << BIT_OFFSET(n));
}

int get_bit(uint64_t *words, int n)
{
    uint64_t bit = words[WORD_OFFSET(n)] & (1 << BIT_OFFSET(n));
    return bit != 0;
}
