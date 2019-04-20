/* Many of the following #defines are completely unused for the
nonce.  For each character,  its code point in code page 437,
Unicode,  and page 8859-1 are given.  The first is used for
non-wide builds in Win32 console,  DOS,  SDL,  and OS/2.
Unicode is used for all wide builds,  and for the non-wide
build of WinGUI.  Code page 8859-1 is used for non-wide X11.

   All of these characters exist in CP437 and Unicode.  Some
don't exist in 8859-1,  in which case the last column is 'TBD'.
Only 32 are used in ncurses.  So caution is advised. */

#ifdef USE_ISO8859_CHARSET
   #define CHOOSE( A, B, C)   (C)
   #define TBD '!'
#else
   #define CHOOSE( A, B, C) (USE_UNICODE_ACS_CHARS ? B : A)
#endif

/* Codes found from https://en.wikipedia.org/wiki/Code_page_437 */

#define SMILE                             CHOOSE( 0x01,  0x263a,  'O')
#define REV_SMILE                         CHOOSE( 0x02,  0x263b,  'O')
#define HEART                             CHOOSE( 0x03,  0x2665,  'H')
#define DIAMOND                           CHOOSE( 0x04,  0x2666, 0x01)
#define CLUB                              CHOOSE( 0x05,  0x2663,  'C')
#define SPADE                             CHOOSE( 0x06,  0x2660,  'S')
#define MEDIUM_BULLET                     CHOOSE( 0x07,  0x2022, 0xb7)
#define REV_BULLET                        CHOOSE( 0x08,  0x2508, 0xb7)
#define WHITE_BULLET                      CHOOSE( 0x09,  0x25cb,    7)
#define REV_WHITE_BULLET                  CHOOSE( 0x0a,  0x25D9,    7)
#define MALE_SYM                          CHOOSE( 0x0b,  0x2642,  'm')
#define FEMALE_SYM                        CHOOSE( 0x0c,  0x2640,  'f')
#define QTR_NOTE                          CHOOSE( 0x0d,  0x266a, 0xbc)
#define EIGHTH_NOTE                       CHOOSE( 0x0e,  0x266b, 0xbd)
#define SPLAT                             CHOOSE( 0x0f,  0xa4  , 0xa4)
#define RIGHT_TRIANGLE                    CHOOSE( 0x10,  0x25b6,  '>')
#define LEFT_TRIANGLE                     CHOOSE( 0x11,  0x25c0,  '<')
#define UP_DOWN_ARROW                     CHOOSE( 0x12,  0x2195, 0x19)
#define DBL_BANG                          CHOOSE( 0x13,  0x203c,  '!')
#define PILCROW                           CHOOSE( 0x14,  0xb6  , 0xb6)
#define SECTION_SIGN                      CHOOSE( 0x15,  0xa7  , 0xa7)
#define LOW_QTR_BLOCK                     CHOOSE( 0x16,  0x25b2,  '_')
#define UP_DOWN_ARROW_UNDERSCORED         CHOOSE( 0x17,  0x21ab, 0x19)
#define UP_ARROW                          CHOOSE( 0x18,  0x2191,  '^')
#define DOWN_ARROW                        CHOOSE( 0x19,  0x2193,  'v')
#define RIGHT_ARROW                       CHOOSE( 0x1a,  0x2192,  '>')
#define LEFT_ARROW                        CHOOSE( 0x1b,  0x2190,  '<')
#define RIGHT_ANGLE                       CHOOSE( 0x1c,  0x221f,  0xe)
#define LEFT_RIGHT_ARROW                  CHOOSE( 0x1d,  0x2194,  '-')
#define UP_TRIANGLE                       CHOOSE( 0x1e,  0x25b2,  '^')
#define DOWN_TRIANGLE                     CHOOSE( 0x1f,  0x25bc,  'v')

#define UPPERCASE_C_CEDILLA               CHOOSE( 0x80,  0xc7  , 0xc7)
#define LOWERCASE_U_UMLAUT                CHOOSE( 0x81,  0xfc  , 0xfc)
#define LOWERCASE_E_ACUTE                 CHOOSE( 0x82,  0xe9  , 0xe9)
#define LOWERCASE_A_CIRCUMFLEX            CHOOSE( 0x83,  0xe2  , 0xe2)
#define LOWERCASE_A_UMLAUT                CHOOSE( 0x84,  0xe4  , 0xe4)
#define LOWERCASE_A_GRAVE                 CHOOSE( 0x85,  0xe0  , 0xea)
#define LOWERCASE_A_RING                  CHOOSE( 0x86,  0xe5  , 0xe5)
#define LOWERCASE_C_CEDILLA               CHOOSE( 0x87,  0xe7  , 0xe7)
#define LOWERCASE_E_CIRCUMFLEX            CHOOSE( 0x88,  0xea  , 0xea)
#define LOWERCASE_E_UMLAUT                CHOOSE( 0x89,  0xeb  , 0xeb)
#define LOWERCASE_E_GRAVE                 CHOOSE( 0x8a,  0xe8  , 0xe8)
#define LOWERCASE_I_UMLAUT                CHOOSE( 0x8b,  0xef  , 0xef)
#define LOWERCASE_I_CIRCUMFLEX            CHOOSE( 0x8c,  0xee  , 0xee)
#define LOWERCASE_I_GRAVE                 CHOOSE( 0x8d,  0xec  , 0xce)
#define UPPERCASE_A_UMLAUT                CHOOSE( 0x8e,  0xc4  , 0xc4)
#define UPPERCASE_A_RING                  CHOOSE( 0x8f,  0xc5  , 0xc5)

#define UPPERCASE_E_ACUTE                 CHOOSE( 0x90,  0xc9  , 0xc9)
#define LOWERCASE_AE_LIGATURE             CHOOSE( 0x91,  0xe6  , 0xe6)
#define UPPERCASE_AE_LIGATURE             CHOOSE( 0x92,  0xc6  , 0xc6)
#define LOWERCASE_O_CIRCUMFLEX            CHOOSE( 0x93,  0xf4  , 0xf4)
#define LOWERCASE_O_UMLAUT                CHOOSE( 0x94,  0xf6  , 0xf6)
#define LOWERCASE_O_GRAVE                 CHOOSE( 0x95,  0xf2  , 0xf2)
#define LOWERCASE_U_CIRCUMFLEX            CHOOSE( 0x96,  0xfb  , 0xfb)
#define LOWERCASE_U_GRAVE                 CHOOSE( 0x97,  0xf9  , 0xf9)
#define LOWERCASE_Y_UMLAUT                CHOOSE( 0x98,  0xff  , 0xff)
#define UPPERCASE_O_UMLAUT                CHOOSE( 0x99,  0xd6  , 0xd6)
#define UPPERCASE_U_UMLAUT                CHOOSE( 0x9a,  0xdc  , 0xdc)
#define CENT_SIGN                         CHOOSE( 0x9b,  0xa2  , 0xa2)
#define STERLING_SIGN                     CHOOSE( 0x9c,  0xa3  ,   30)
#define YEN_SIGN                          CHOOSE( 0x9d,  0xa5  , 0xa5)
#define PESETA_SIGN                       CHOOSE( 0x9e,  0x20a7,  TBD)
#define F_WITH_HOOK                       CHOOSE( 0x9f,  0x0192,  TBD)

#define LOWERCASE_A_ACUTE                 CHOOSE( 0xa0,  0xe1  , 0xe1)
#define LOWERCASE_I_ACUTE                 CHOOSE( 0xa1,  0xed  , 0xed)
#define LOWERCASE_O_ACUTE                 CHOOSE( 0xa2,  0xf3  , 0xf3)
#define LOWERCASE_U_ACUTE                 CHOOSE( 0xa3,  0xfa  , 0xfa)
#define LOWERCASE_N_TILDE                 CHOOSE( 0xa4,  0xf1  , 0xf1)
#define UPPERCASE_N_TILDE                 CHOOSE( 0xa5,  0xd1  , 0xd1)
#define A_ORDINAL                         CHOOSE( 0xa6,  0xaa  , 0xaa)
#define O_ORDINAL                         CHOOSE( 0xa7,  0xba  , 0xba)
#define INVERTED_QUESTION_MARK            CHOOSE( 0xa8,  0xbf  , 0xbf)
#define REVERSED_NOT_SIGN                 CHOOSE( 0xa9,  0x2310,  TBD)
#define NOT_SIGN                          CHOOSE( 0xaa,  0xac  , 0xac)
#define VULGAR_HALF                       CHOOSE( 0xab,  0xbd  , 0xbd)
#define VULGAR_QUARTER                    CHOOSE( 0xac,  0xbc  , 0xbc)
#define INVERTED_EXCLAMATION_MARK         CHOOSE( 0xad,  0xa1  , 0xa1)
#define LEFT_ANGLE_QUOTE_MARK             CHOOSE( 0xae,  0xab  , 0xab)
#define RIGHT_ANGLE_QUOTE_MARK            CHOOSE( 0xaf,  0xbb  , 0xbb)

#define LIGHT_SHADE                       CHOOSE( 0xb0,  0x2591, '#' )
#define MEDIUM_SHADE                      CHOOSE( 0xb1,  0x2592,    2)
#define DARK_SHADE                        CHOOSE( 0xb2,  0x2593,  TBD)
#define BOX_VLINE                         CHOOSE( 0xb3,  0x2502,   25)
#define BOX_RTEE                          CHOOSE( 0xb4,  0x2524,   22)
#define BOX_SD_RTEE                       CHOOSE( 0xb5,  0x2561,   22)
#define BOX_DS_RTEE                       CHOOSE( 0xb6,  0x2562,   22)
#define BOX_DS_URCORNER                   CHOOSE( 0xb7,  0x2556,   12)
#define BOX_SD_URCORNER                   CHOOSE( 0xb8,  0x2555,   12)
#define BOX_D_RTEE                        CHOOSE( 0xb9,  0x2563,   22)
#define BOX_D_VLINE                       CHOOSE( 0xba,  0x2551,   25)
#define BOX_D_URCORNER                    CHOOSE( 0xbb,  0x2557,   12)
#define BOX_D_LRCORNER                    CHOOSE( 0xbc,  0x255D,   11)
#define BOX_DS_LRCORNER                   CHOOSE( 0xbd,  0x255c,   11)
#define BOX_SD_LRCORNER                   CHOOSE( 0xbe,  0x255b,   11)
#define BOX_URCORNER                      CHOOSE( 0xbf,  0x2510,   12)

#define BOX_LLCORNER                      CHOOSE( 0xc0,  0x2514,   14)
#define BOX_BTEE                          CHOOSE( 0xc1,  0x2534,   23)
#define BOX_TTEE                          CHOOSE( 0xc2,  0x252c,   24)
#define BOX_LTEE                          CHOOSE( 0xc3,  0x251c,   21)
#define BOX_HLINE                         CHOOSE( 0xc4,  0x2500,   18)
#define BOX_PLUS                          CHOOSE( 0xc5,  0x253c,   15)
#define BOX_SD_LTEE                       CHOOSE( 0xc6,  0x255e,   21)
#define BOX_DS_LTEE                       CHOOSE( 0xc7,  0x255f,   21)
#define BOX_D_LLCORNER                    CHOOSE( 0xc8,  0x255A,   14)
#define BOX_D_ULCORNER                    CHOOSE( 0xc9,  0x2554,   13)
#define BOX_D_BTEE                        CHOOSE( 0xca,  0x2569,   23)
#define BOX_D_TTEE                        CHOOSE( 0xcb,  0x2566,   24)
#define BOX_D_LTEE                        CHOOSE( 0xcc,  0x2560,   21)
#define BOX_D_HLINE                       CHOOSE( 0xcd,  0x2550,   18)
#define BOX_D_PLUS                        CHOOSE( 0xce,  0x256C,   15)
#define BOX_SD_BTEE                       CHOOSE( 0xcf,  0x2567,   23)

#define BOX_DS_BTEE                       CHOOSE( 0xd0,  0x2568,   23)
#define BOX_SD_TTEE                       CHOOSE( 0xd1,  0x2564,   24)
#define BOX_DS_TTEE                       CHOOSE( 0xd2,  0x2565,   24)
#define BOX_DS_LLCORNER                   CHOOSE( 0xd3,  0x2559,   14)
#define BOX_SD_LLCORNER                   CHOOSE( 0xd4,  0x2558,   14)
#define BOX_SD_ULCORNER                   CHOOSE( 0xd5,  0x2552,   13)
#define BOX_DS_ULCORNER                   CHOOSE( 0xd6,  0x2553,   13)
#define BOX_DS_PLUS                       CHOOSE( 0xd7,  0x256b,   15)
#define BOX_SD_PLUS                       CHOOSE( 0xd8,  0x256a,   15)
#define BOX_LRCORNER                      CHOOSE( 0xd9,  0x2518,   11)
#define BOX_ULCORNER                      CHOOSE( 0xda,  0x250c,   13)
#define FULL_BLOCK                        CHOOSE( 0xdb,  0x2588,    0)
#define LOWER_HALF_BLOCK                  CHOOSE( 0xdc,  0x2584,  TBD)
#define LEFT_HALF_BLOCK                   CHOOSE( 0xdd,  0x258c,  TBD)
#define RIGHT_HALF_BLOCK                  CHOOSE( 0xde,  0x2590,  TBD)
#define UPPER_HALF_BLOCK                  CHOOSE( 0xdf,  0x2580,  TBD)

#define ALPHA                             CHOOSE( 0xe0,  0x03b1,  TBD)
#define BETA                              CHOOSE( 0xe1,  0x00df,  TBD)
#define GAMMA                             CHOOSE( 0xe2,  0x0393,  TBD)
#define PI                                CHOOSE( 0xe3,  0x03c0,   28)
#define UPPERCASE_SIGMA                   CHOOSE( 0xe4,  0x03a3,  TBD)
#define LOWERCASE_SIGMA                   CHOOSE( 0xe5,  0x03c3,  TBD)
#define MU                                CHOOSE( 0xe6,  0x00b5, 0xb5)
#define TAU                               CHOOSE( 0xe7,  0x03c4,  TBD)
#define UPPERCASE_PHI                     CHOOSE( 0xe8,  0x03a6,  TBD)
#define THETA                             CHOOSE( 0xe9,  0x0398,  TBD)
#define OMEGA                             CHOOSE( 0xea,  0x03a9,  TBD)
#define DELTA                             CHOOSE( 0xeb,  0x03b4,  TBD)
#define INFINITY_SIGN                     CHOOSE( 0xec,  0x221e,  TBD)
#define LOWERCASE_PHI                     CHOOSE( 0xed,  0x03c6,  TBD)
#define EPSILON                           CHOOSE( 0xee,  0x03b5,  TBD)
#define INTERSECTION                      CHOOSE( 0xef,  0x2229,  TBD)

#define TRIPLE_BAR                        CHOOSE( 0xf0,  0x2261,  TBD)
#define PLUS_OR_MINUS                     CHOOSE( 0xf1,  0x00b1,    8)
#define GREATER_THAN_OR_EQUAL_TO          CHOOSE( 0xf2,  0x2265,   27)
#define LESSER_THAN_OR_EQUAL_TO           CHOOSE( 0xf3,  0x2264,   26)
#define UPPER_HALF_INTEGRAL_SIGN          CHOOSE( 0xf4,  0x2320,  TBD)
#define LOWER_HALF_INTEGRAL_SIGN          CHOOSE( 0xf5,  0x2321,  TBD)
#define DIVISION_SIGN                     CHOOSE( 0xf6,  0x00f7, 0xf7)
#define APPROXIMATELY_EQUALS_SIGN         CHOOSE( 0xf7,  0x2248,  TBD)
#define DEGREE_SIGN                       CHOOSE( 0xf8,  0x00b0, 0xb0)
#define LARGE_BULLET                      CHOOSE( 0xf9,  0x2219,    7)
#define SMALL_BULLET                      CHOOSE( 0xfa,  0x00b7, 0xb7)
#define SQUARE_ROOT                       CHOOSE( 0xfb,  0x221a,  TBD)
#define SUPERSCRIPT_N                     CHOOSE( 0xfc,  0x207f,  TBD)
#define SUPERSCRIPT_2                     CHOOSE( 0xfd,  0x00b2, 0xb2)
#define CENTERED_SQUARE                   CHOOSE( 0xfe,  0x25a0,  TBD)
#define NON_BREAKING_SPACE                CHOOSE( 0xff,  0x00a0,  TBD)



      /* It says at http://unicode.org/charts/PDF/U2300.pdf */
      /* that '...the scan line numbers here refer to old,  */
      /* low-resolution technology for terminals, with only */
      /* nine scan lines per fixed-size character glyph.    */
      /* Even-numbered scan lines are unified with box      */
      /* drawing graphics."                                 */
      /*  The utility of these is questionable;  they'd     */
      /* work Just Fine in wingdi (_if_ the appropriate     */
      /* glyphs are available),  but not elsewhere.         */
#define HORIZ_SCAN_LINE_1                 CHOOSE( 0x2d,  0x23ba,   16)
#define HORIZ_SCAN_LINE_3                 CHOOSE( 0x2d,  0x23bb,   17)
#define HORIZ_SCAN_LINE_7                 CHOOSE( 0x2d,  0x23bc,   19)
#define HORIZ_SCAN_LINE_9                 CHOOSE(  '_',  0x23bd,   20)

      /* Code page 437 lacks a 'for real' not-equals,  so for that, */
      /* we use the double-horizontal single-vertical box drawing : */
#define NOT_EQUALS_SIGN                   CHOOSE( 0xd8, 0x2260,    29)

# define A(x) ((chtype)x | A_ALTCHARSET)

chtype acs_map[128] =
{
   A(0), A(1), A(2), A(3), A(4), A(5), A(6), A(7), A(8),
   A(9), A(10),
   CLUB, HEART, SPADE, SMILE, REV_SMILE,                      /* 11 12 13 14 15 */
   MEDIUM_BULLET, WHITE_BULLET, PILCROW, SECTION_SIGN,        /* 16 17 18 19 */
   A_ORDINAL, O_ORDINAL, LOWERCASE_PHI,                       /* 20 21 22 */
   INVERTED_EXCLAMATION_MARK, INVERTED_QUESTION_MARK,         /* 23 24 */
   REVERSED_NOT_SIGN, NOT_SIGN,                               /* 25 26 */
   UPPER_HALF_INTEGRAL_SIGN, LOWER_HALF_INTEGRAL_SIGN,        /* 27 28   */
   SUPERSCRIPT_N, CENTERED_SQUARE, F_WITH_HOOK,               /* 29 30 31 */

   RIGHT_ARROW, LEFT_ARROW, UP_ARROW, DOWN_ARROW,             /* 32 !"# */

   PI, NOT_EQUALS_SIGN,  VULGAR_HALF, VULGAR_QUARTER,         /* $%&' */
   '(',
   LEFT_ANGLE_QUOTE_MARK, RIGHT_ANGLE_QUOTE_MARK,             /* )* */
   DARK_SHADE, SUPERSCRIPT_2, INFINITY_SIGN,                  /* +,- */
   ALPHA, BETA, GAMMA, UPPERCASE_SIGMA, LOWERCASE_SIGMA,      /* ./012  */
   '3',
   MU, TAU, UPPERCASE_PHI, THETA, OMEGA, DELTA, EPSILON,      /* 456789: */

   BOX_SD_LRCORNER, BOX_SD_URCORNER, BOX_SD_ULCORNER,         /* ;<= */
   BOX_SD_LLCORNER, BOX_SD_PLUS,                              /* >? */
   BOX_SD_LTEE, BOX_SD_RTEE, BOX_SD_BTEE, BOX_SD_TTEE,        /* @ABC */

   BOX_D_LRCORNER, BOX_D_URCORNER, BOX_D_ULCORNER,             /* DEF */
   BOX_D_LLCORNER, BOX_D_PLUS,                                 /* GH */
   BOX_D_LTEE, BOX_D_RTEE, BOX_D_BTEE, BOX_D_TTEE,             /* IJKL */

   BOX_DS_LRCORNER, BOX_DS_URCORNER, BOX_DS_ULCORNER,          /* MNO */
   BOX_DS_LLCORNER, BOX_DS_PLUS,                               /* PQ */
   BOX_DS_LTEE, BOX_DS_RTEE, BOX_DS_BTEE, BOX_DS_TTEE,         /* RSTU */

   BOX_LRCORNER, BOX_URCORNER, BOX_ULCORNER,                   /* VWX */
   BOX_LLCORNER, BOX_PLUS,                                     /* YZ */
   BOX_LTEE, BOX_RTEE, BOX_BTEE, BOX_TTEE,                     /* [\]^ */

   BOX_HLINE, BOX_VLINE, BOX_D_HLINE, BOX_D_VLINE,             /* _`ab */

   DIVISION_SIGN, APPROXIMATELY_EQUALS_SIGN,                   /* cd */
   INTERSECTION, TRIPLE_BAR,                                   /* ef */
   SMALL_BULLET, LARGE_BULLET, SQUARE_ROOT,                    /* ghi */
   DIAMOND, MEDIUM_SHADE,                                      /* jk */
   HORIZ_SCAN_LINE_1, HORIZ_SCAN_LINE_3,                       /* lm */
   HORIZ_SCAN_LINE_7, HORIZ_SCAN_LINE_9,                       /* no */
   UPPER_HALF_BLOCK, LOWER_HALF_BLOCK,                         /* pq */
   LEFT_HALF_BLOCK, RIGHT_HALF_BLOCK, FULL_BLOCK,              /* rst */
   LESSER_THAN_OR_EQUAL_TO, GREATER_THAN_OR_EQUAL_TO,          /* uv */
   DEGREE_SIGN, PLUS_OR_MINUS, LIGHT_SHADE, SPLAT,             /* wxyz */
   CENT_SIGN, YEN_SIGN, PESETA_SIGN, STERLING_SIGN,            /* {|}~ */
    A(127)
};

# undef A
