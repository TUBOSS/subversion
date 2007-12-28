#!/usr/bin/env python
"""getctype.py - Generate the svn_ctype character classification table.
"""

import string

# Table of ASCII character names
names = ('nul', 'soh', 'stx', 'etx', 'eot', 'enq', 'ack', 'bel',
         'bs',  'ht',  'nl',  'vt',  'np',  'cr',  'so',  'si',
         'dle', 'dc1', 'dc2', 'dc3', 'dc4', 'nak', 'syn', 'etb',
         'can', 'em',  'sub', 'esc', 'fs',  'gs',  'rs',  'us',
         'sp',  '!',   '"',   '#',   '$',   '%',   '&',   '\'',
         '(',   ')',   '*',   '+',   ',',   '-',   '.',   '/',
         '0',   '1',   '2',   '3',   '4',   '5',   '6',   '7',
         '8',   '9',   ':',   ';',   '<',   '=',   '>',   '?',
         '@',   'A',   'B',   'C',   'D',   'E',   'F',   'G',
         'H',   'I',   'J',   'K',   'L',   'M',   'N',   'O',
         'P',   'Q',   'R',   'S',   'T',   'U',   'V',   'W',
         'X',   'Y',   'Z',   '[',   '\\',  ']',   '^',   '_',
         '`',   'a',   'b',   'c',   'd',   'e',   'f',   'g',
         'h',   'i',   'j',   'k',   'l',   'm',   'n',   'o',
         'p',   'q',   'r',   's',   't',   'u',   'v',   'w',
         'x',   'y',   'z',   '{',   '|',   '}',   '~',   'del')

# All whitespace characters:
#   horizontal tab, vertical tab, new line, form feed, carriage return, space
whitespace = (9, 10, 11, 12, 13, 32)

# Bytes not valid in UTF-8 sequences
utf8_invalid = (0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF)

print '    /* **** DO NOT EDIT! ****'
print '       This table was generated by genctype.py, make changes there. */'

for c in xrange(256):
    bits = []

    # Ascii subrange
    if c < 128:
        bits.append('SVN_CTYPE_ASCII')

        if len(names[c]) == 1:
            name = string.center(names[c], 3)
        else:
            name = string.ljust(names[c], 3)

        # Control characters
        if c < 32 or c == 127:
            bits.append('SVN_CTYPE_CNTRL')

        # Whitespace characters
        if c in whitespace:
            bits.append('SVN_CTYPE_SPACE')

        # Punctuation marks
        if c >= 33 and c < 48 \
           or c >= 58 and c < 65 \
           or c >= 91 and c < 97 \
           or c >= 123 and c < 127:
            bits.append('SVN_CTYPE_PUNCT')

        # Decimal digits
        elif c >= 48 and c < 58:
            bits.append('SVN_CTYPE_DIGIT')

        # Uppercase letters
        elif c >= 65 and c < 91:
            bits.append('SVN_CTYPE_UPPER')
            # Hexadecimal digits
            if c <= 70:
                bits.append('SVN_CTYPE_XALPHA')

        # Lowercase letters
        elif c >= 97 and c < 123:
            bits.append('SVN_CTYPE_LOWER')
            # Hexadecimal digits
            if c <= 102:
                bits.append('SVN_CTYPE_XALPHA')

    # UTF-8 multibyte sequences
    else:
        name = hex(c)[1:]

        # Lead bytes (start of sequence)
        if c > 0xC0 and c < 0xFE and c not in utf8_invalid:
            bits.append('SVN_CTYPE_UTF8LEAD')

        # Continuation bytes
        elif (c & 0xC0) == 0x80:
            bits.append('SVN_CTYPE_UTF8CONT')

    if len(bits) == 0:
        flags = '0'
    else:
        flags = string.join(bits, ' | ')
    print '    /* %s */ %s,' % (name, flags)
