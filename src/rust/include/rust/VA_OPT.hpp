//  Copyright (c) 2019 Will Wray https://keybase.io/willwray
//  Copyright (c) 2020 The Zcash developers
//
//  Distributed under the Boost Software License, Version 1.0.
//        http://www.boost.org/LICENSE_1_0.txt
//
//  Repo: https://github.com/willwray/VA_OPT

#ifndef ZCASH_RUST_INCLUDE_RUST_VA_OPT_H
#define ZCASH_RUST_INCLUDE_RUST_VA_OPT_H

/*
  VA_OPT.hpp
  ==========

  Preprocessor utilities for testing emptiness of macro arguments
  and for conditional expansion based on the emptiness of ARGS.

  VA_OPT_SUPPORT(?)
      1 if __VA_OPT__ support is detected else 0 (see platform note).

  C++20's __VA_OPT__ finally provides a reliable test for empty ARGS.
  The following macros use __VA_OPT__, if detected, otherwise they
  provide 'polyfill' fallbacks (though with some failing edge cases).

  IS_EMPTY(...)
      1 if the ... ARGS is empty else 0.

  IFN(...)
      If ... ARGS are not empty then a trailing 'paren expression' (X)
      is deparenthesized to X else the trailing (X) term is consumed.
      E.g. IFN()(N) vanishes, while IFN(NotEmpty)(N) -> N

      In other words, IFN(ARGS) is like __VA_OPT__, but with explicit
      (ARGS) in place of an implicit __VA_ARGS__ check.

  IFE(...)
      If ... ARGS is empty expand trailing 'paren expression' (X) to X
      else if ARGS are not empty consume the trailing paren expression.
      E.g. IFE(NotEmpty)(E) vanishes, while IFE()(E) -> E

  IFNE(...)(N,E...)
      If ... ARGS are not empty expands to N else expands to E...
      E.g. IFNE(ARG)(X,()) is equivalent to IFN(ARG)(X)IFE(ARG)(())
      both put back a terminating () removed by the outer macro call.

  Without VA_OPT_SUPPORT these 'emptiness' macros are not perfect;
  IS_EMPTY, IFN, IFE, IFNE may cause a compile error ('too few args')
  if the argument is a function-like macro name that expects ARG(s).

  IBP(...)
      IS_BEGIN_PARENS macro to test if an argument is parenthesised:
      1 if ... ARGS begins with a 'paren expression' else 0.

  Platform note: Current Sept 2019 __VA_OPT__ support:
  -------------
  Clang -std=c++2a enables it. GCC has it enabled without -std=c++2a
  but warns "__VA_OPT__ is not available until C++2a" if another -std
  flag is supplied along with -pedantic (dont know how to suppress it).
  MSVC TBD

  Credits
  -------
  Props to pre-pro pioneers, particularly Paul Mensonides.
  The 'emptiness' methods are adapted from BOOST_VMD_IS_EMPTY which,
. in turn, depends on BOOST Preprocessor's BOOST_PP_IS_BEGIN_PARENS
  (adapted and exposed here as IBP 'Is Begin Parens'):
  www.boost.org/doc/libs/1_71_0/libs/vmd
  www.boost.org/doc/libs/1_71_0/libs/preprocessor
*/

#define VA_ARG1(A0,A1,...) A1
// VA_EMPTY works only if __VA_OPT__ is supported, else always -> 1
#define VA_EMPTY(...) VA_ARG1(__VA_OPT__(,)0,1,)

// VA_OPT_SUPPORT helper macro for __VA_OPT__ feature detection.
// Adapted from https://stackoverflow.com/a/48045656/7443483
// Use as #if VA_OPT_SUPPORT(?)
#define VA_OPT_SUPPORT ! VA_EMPTY

#if VA_OPT_SUPPORT(?)

#  define IS_EMPTY(...) VA_EMPTY(__VA_ARGS__)
#  define IFN(...) VA_EAT __VA_OPT__(()VA_IDENT)
#  define IFE(...) VA_IDENT __VA_OPT__(()VA_EAT)
#  define IFNE(...) VA_ARGTAIL __VA_OPT__((,)VA_ARG0)

#else

#  define IS_EMPTY(...) IFP(IBP(__VA_ARGS__))(IE_GEN_0,IE_IBP)(__VA_ARGS__)
#  define IFN(...) IFP(IBP(__VA_ARGS__))(GEN_IDENT,EAT_OR_IDENT)(__VA_ARGS__)
#  define IFE(...) IFP(IBP(__VA_ARGS__))(GEN_EAT,IDENT_OR_EAT)(__VA_ARGS__)
#  define IFNE(...) IFP(IBP(__VA_ARGS__))(GEN_ARGTAIL,ARG0_OR_TAIL)(__VA_ARGS__)

#endif

#define VA_EAT(...)
#define VA_IDENT(...) __VA_ARGS__
#define VA_ARG0_(A0,...) A0
#define VA_ARG0(...) VA_ARG0_(__VA_ARGS__)
#define VA_ARGTAIL_(A0,...) __VA_ARGS__
#define VA_ARGTAIL(...) VA_ARGTAIL_(__VA_ARGS__)

// IFP helper macros to test IBP for IFN and IS_EMPTY
#define IFP_0(T,...) __VA_ARGS__
#define IFP_1(T,...) T

#define IFP_CAT(A,...) A##__VA_ARGS__
#define IFP(BP) IFP_CAT(IFP_,BP)

// IS_BEGIN_PAREN helper macros adapted from BOOST VMD
#define IBP_CAT_(A,...) A##__VA_ARGS__
#define IBP_CAT(A,...) IBP_CAT_(A,__VA_ARGS__)

#define IBP_ARG0_(A,...) A
#define IBP_ARG0(...) IBP_ARG0_(__VA_ARGS__)

#define IBP_IS_ARGS(...) 1

#define IBP_1 1,
#define IBP_IBP_IS_ARGS 0,

// IBP IS_BEGIN_PAREN returns 1 or 0 if ... ARGS is parenthesised
#define IBP(...) IBP_ARG0(IBP_CAT(IBP_, IBP_IS_ARGS __VA_ARGS__))

// IFN, IFE, IFNE and IF_EMPTY helpers without __VA_OPT__ support
#if ! VA_OPT_SUPPORT(?)

#  define IBP_(T,...) IBP_ARG0(IBP_CAT(IF##T##_, IBP_IS_ARGS __VA_ARGS__))

   // IS_EMPTY helper macros, depend on IBP
#  define IE_REDUCE_IBP(...) ()
#  define IE_GEN_0(...) 0
#  define IE_IBP(...) IBP(IE_REDUCE_IBP __VA_ARGS__ ())

#  define GEN_IDENT(...) VA_IDENT
#  define GEN_EAT(...) VA_EAT
#  define GEN_ARGTAIL(...) VA_ARGTAIL
#  define GEN_ARG0(...) VA_ARG0

   // IFN, IFE, IFNE helper macros
#  define EAT_OR_IDENT(...) IBP_(N,IE_REDUCE_IBP __VA_ARGS__ ())
#  define IFN_1 VA_EAT,
#  define IFN_IBP_IS_ARGS VA_IDENT,

#  define IDENT_OR_EAT(...) IBP_(E,IE_REDUCE_IBP __VA_ARGS__ ())
#  define IFE_1 VA_IDENT,
#  define IFE_IBP_IS_ARGS VA_EAT,

#  define ARG0_OR_TAIL(...) IBP_(NE,IE_REDUCE_IBP __VA_ARGS__ ())
#  define IFNE_1 VA_ARGTAIL,
#  define IFNE_IBP_IS_ARGS VA_ARG0,

#endif // IFN and IF_EMPTY defs

#endif // ZCASH_RUST_INCLUDE_RUST_VA_OPT_H
