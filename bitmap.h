// code was inspired by https://stackoverflow.com/questions/1225998/what-is-a-bitmap-in-c

enum
{
    BITS_PER_WORD = sizeof(uint64_t) * 8
};
#define BIT_OFFSET(b) ((b) % BITS_PER_WORD)

void set_bit(uint64_t *words, int n)
{
    *words |= (((uint64_t)1) << BIT_OFFSET(n));
}

void clear_bit(uint64_t *words, int n)
{
    *words &= ~(((uint64_t)1) << BIT_OFFSET(n));
}

int get_bit(uint64_t *words, int n)
{
    uint64_t bit = *words & (((uint64_t)1) << BIT_OFFSET(n));
    return bit != 0;
}

/** shift all bits after n to the left **/
static inline void shift_one_left(uint64_t *word, unsigned int n)
{
    uint64_t msk = (((uint64_t)1) << n) - 1;
    uint64_t cpy = *word << 1;
    *word = (cpy & ~msk) ^ (*word & msk);
}

static inline void shift_right(uint64_t *word, unsigned int n)
{
    *word >>= n;
}
