#include "language_layer.h"


// Created by Björn Höhrmann
// @see http://bjoern.hoehrmann.de/utf-8/decoder/dfa/
static const u8 utf8d[] = {
    // The first part of the table maps bytes to character classes that
    // to reduce the size of the transition table and create bitmasks.
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
     7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
     8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

    // The second part is a transition table that maps a combination
    // of a state of the automaton and a character class to a state.
    0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
    12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
    12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
    12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
    12,36,12,12,12,12,12,12,12,12,12,12,
};

#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static void utf8_decode(u32* state, u32* rune, u32 byte) {
    u32 type = utf8d[byte];

    *rune = (*state != UTF8_ACCEPT) ? (byte & 0x3FU) | (*rune << 6) : (0xFF >> type) & (byte);

    *state = utf8d[256 + *state + type];
}

Rune_Iterator make_rune_iterator(String the_string) {
    return (Rune_Iterator) {
        the_string,
        UTF8_ACCEPT,
        0,
        0,
    };
}

b32 can_step_rune_iterator(Rune_Iterator iter) {
    return (
        iter.the_string.data && 
        iter.the_string.len && 
        iter.index < iter.the_string.len && 
        iter.decoder_state != UTF8_REJECT
    );
}

void step_rune_iterator(Rune_Iterator* iter) {
    assert(can_step_rune_iterator(*iter));

    for (; iter->index < iter->the_string.len; ++iter->index) {
        const u8 c = iter->the_string.data[iter->index];
        utf8_decode(&iter->decoder_state, &iter->rune, c);

        if (iter->decoder_state == UTF8_REJECT) return;

        if (iter->decoder_state == UTF8_ACCEPT) break;
    }

    iter->index += 1;
}

Rune rune(Rune_Iterator iter) {
    step_rune_iterator(&iter);
    return iter.rune;
}

Rune peek_rune(Rune_Iterator iter) {
    step_rune_iterator(&iter);
    if (!can_step_rune_iterator(iter)) return 0;
    step_rune_iterator(&iter);
    return iter.rune;
}

int rune_count(String the_string) {
    int count = 0;
    for (rune_iterator(the_string)) count++;
    return count;
}

String advance_string(String the_string, int amount) {
    assert(amount < the_string.len);
    return (String) { the_string.data + amount, the_string.len - amount, null_allocator() };
}

int find_from_left(String the_string, Rune r) {
    for (rune_iterator(the_string)) {
        const Rune o = rune(iter);
        if (o == r) return iter.index;
    }

    return -1;
}

b32 string_equal(String a, String b) {
    if (a.len != b.len) return false;
    for (int i = 0; i < a.len; ++i) {
        if (a.data[i] != b.data[i]) return false;
    }
    return true;
}

String copy_string(String a, Allocator allocator) {
    u8* const data = mem_alloc_array(allocator, u8, a.len + 1);
    mem_copy(data, a.data, a.len);
    data[a.len] = 0;
    return (String) { data, a.len, allocator };
}

b32 starts_with(String a, String b) {
    if (b.len > a.len) return false;

    for (int i = 0; i < b.len; ++i) {
        if (a.data[i] != b.data[i]) return false;
    }

    return true;
}
