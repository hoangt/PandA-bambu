/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_PARSER_H_INCLUDED
# define YY_YY_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    CONSTANTTOKEN = 258,
    MIDPOINTCONSTANTTOKEN = 259,
    DYADICCONSTANTTOKEN = 260,
    HEXCONSTANTTOKEN = 261,
    HEXADECIMALCONSTANTTOKEN = 262,
    BINARYCONSTANTTOKEN = 263,
    PITOKEN = 264,
    IDENTIFIERTOKEN = 265,
    STRINGTOKEN = 266,
    LPARTOKEN = 267,
    RPARTOKEN = 268,
    LBRACKETTOKEN = 269,
    RBRACKETTOKEN = 270,
    EQUALTOKEN = 271,
    ASSIGNEQUALTOKEN = 272,
    COMPAREEQUALTOKEN = 273,
    COMMATOKEN = 274,
    EXCLAMATIONTOKEN = 275,
    SEMICOLONTOKEN = 276,
    STARLEFTANGLETOKEN = 277,
    LEFTANGLETOKEN = 278,
    RIGHTANGLEUNDERSCORETOKEN = 279,
    RIGHTANGLEDOTTOKEN = 280,
    RIGHTANGLESTARTOKEN = 281,
    RIGHTANGLETOKEN = 282,
    DOTSTOKEN = 283,
    DOTTOKEN = 284,
    QUESTIONMARKTOKEN = 285,
    VERTBARTOKEN = 286,
    ATTOKEN = 287,
    DOUBLECOLONTOKEN = 288,
    COLONTOKEN = 289,
    DOTCOLONTOKEN = 290,
    COLONDOTTOKEN = 291,
    EXCLAMATIONEQUALTOKEN = 292,
    APPROXTOKEN = 293,
    ANDTOKEN = 294,
    ORTOKEN = 295,
    PLUSTOKEN = 296,
    MINUSTOKEN = 297,
    MULTOKEN = 298,
    DIVTOKEN = 299,
    POWTOKEN = 300,
    SQRTTOKEN = 301,
    EXPTOKEN = 302,
    LOGTOKEN = 303,
    LOG2TOKEN = 304,
    LOG10TOKEN = 305,
    SINTOKEN = 306,
    COSTOKEN = 307,
    TANTOKEN = 308,
    ASINTOKEN = 309,
    ACOSTOKEN = 310,
    ATANTOKEN = 311,
    SINHTOKEN = 312,
    COSHTOKEN = 313,
    TANHTOKEN = 314,
    ASINHTOKEN = 315,
    ACOSHTOKEN = 316,
    ATANHTOKEN = 317,
    ABSTOKEN = 318,
    ERFTOKEN = 319,
    ERFCTOKEN = 320,
    LOG1PTOKEN = 321,
    EXPM1TOKEN = 322,
    DOUBLETOKEN = 323,
    SINGLETOKEN = 324,
    HALFPRECISIONTOKEN = 325,
    QUADTOKEN = 326,
    DOUBLEDOUBLETOKEN = 327,
    TRIPLEDOUBLETOKEN = 328,
    DOUBLEEXTENDEDTOKEN = 329,
    CEILTOKEN = 330,
    FLOORTOKEN = 331,
    NEARESTINTTOKEN = 332,
    HEADTOKEN = 333,
    REVERTTOKEN = 334,
    SORTTOKEN = 335,
    TAILTOKEN = 336,
    MANTISSATOKEN = 337,
    EXPONENTTOKEN = 338,
    PRECISIONTOKEN = 339,
    ROUNDCORRECTLYTOKEN = 340,
    PRECTOKEN = 341,
    POINTSTOKEN = 342,
    DIAMTOKEN = 343,
    DISPLAYTOKEN = 344,
    VERBOSITYTOKEN = 345,
    CANONICALTOKEN = 346,
    AUTOSIMPLIFYTOKEN = 347,
    TAYLORRECURSIONSTOKEN = 348,
    TIMINGTOKEN = 349,
    TIMETOKEN = 350,
    FULLPARENTHESESTOKEN = 351,
    MIDPOINTMODETOKEN = 352,
    DIEONERRORMODETOKEN = 353,
    SUPPRESSWARNINGSTOKEN = 354,
    RATIONALMODETOKEN = 355,
    HOPITALRECURSIONSTOKEN = 356,
    ONTOKEN = 357,
    OFFTOKEN = 358,
    DYADICTOKEN = 359,
    POWERSTOKEN = 360,
    BINARYTOKEN = 361,
    HEXADECIMALTOKEN = 362,
    FILETOKEN = 363,
    POSTSCRIPTTOKEN = 364,
    POSTSCRIPTFILETOKEN = 365,
    PERTURBTOKEN = 366,
    MINUSWORDTOKEN = 367,
    PLUSWORDTOKEN = 368,
    ZEROWORDTOKEN = 369,
    NEARESTTOKEN = 370,
    HONORCOEFFPRECTOKEN = 371,
    TRUETOKEN = 372,
    FALSETOKEN = 373,
    DEFAULTTOKEN = 374,
    MATCHTOKEN = 375,
    WITHTOKEN = 376,
    ABSOLUTETOKEN = 377,
    DECIMALTOKEN = 378,
    RELATIVETOKEN = 379,
    FIXEDTOKEN = 380,
    FLOATINGTOKEN = 381,
    ERRORTOKEN = 382,
    QUITTOKEN = 383,
    FALSEQUITTOKEN = 384,
    RESTARTTOKEN = 385,
    LIBRARYTOKEN = 386,
    LIBRARYCONSTANTTOKEN = 387,
    DIFFTOKEN = 388,
    SIMPLIFYTOKEN = 389,
    REMEZTOKEN = 390,
    BASHEVALUATETOKEN = 391,
    FPMINIMAXTOKEN = 392,
    HORNERTOKEN = 393,
    EXPANDTOKEN = 394,
    SIMPLIFYSAFETOKEN = 395,
    TAYLORTOKEN = 396,
    TAYLORFORMTOKEN = 397,
    AUTODIFFTOKEN = 398,
    DEGREETOKEN = 399,
    NUMERATORTOKEN = 400,
    DENOMINATORTOKEN = 401,
    SUBSTITUTETOKEN = 402,
    COEFFTOKEN = 403,
    SUBPOLYTOKEN = 404,
    ROUNDCOEFFICIENTSTOKEN = 405,
    RATIONALAPPROXTOKEN = 406,
    ACCURATEINFNORMTOKEN = 407,
    ROUNDTOFORMATTOKEN = 408,
    EVALUATETOKEN = 409,
    LENGTHTOKEN = 410,
    INFTOKEN = 411,
    MIDTOKEN = 412,
    SUPTOKEN = 413,
    MINTOKEN = 414,
    MAXTOKEN = 415,
    READXMLTOKEN = 416,
    PARSETOKEN = 417,
    PRINTTOKEN = 418,
    PRINTXMLTOKEN = 419,
    PLOTTOKEN = 420,
    PRINTHEXATOKEN = 421,
    PRINTFLOATTOKEN = 422,
    PRINTBINARYTOKEN = 423,
    PRINTEXPANSIONTOKEN = 424,
    BASHEXECUTETOKEN = 425,
    EXTERNALPLOTTOKEN = 426,
    WRITETOKEN = 427,
    ASCIIPLOTTOKEN = 428,
    RENAMETOKEN = 429,
    INFNORMTOKEN = 430,
    SUPNORMTOKEN = 431,
    FINDZEROSTOKEN = 432,
    FPFINDZEROSTOKEN = 433,
    DIRTYINFNORMTOKEN = 434,
    NUMBERROOTSTOKEN = 435,
    INTEGRALTOKEN = 436,
    DIRTYINTEGRALTOKEN = 437,
    WORSTCASETOKEN = 438,
    IMPLEMENTPOLYTOKEN = 439,
    IMPLEMENTCONSTTOKEN = 440,
    CHECKINFNORMTOKEN = 441,
    ZERODENOMINATORSTOKEN = 442,
    ISEVALUABLETOKEN = 443,
    SEARCHGALTOKEN = 444,
    GUESSDEGREETOKEN = 445,
    DIRTYFINDZEROSTOKEN = 446,
    IFTOKEN = 447,
    THENTOKEN = 448,
    ELSETOKEN = 449,
    FORTOKEN = 450,
    INTOKEN = 451,
    FROMTOKEN = 452,
    TOTOKEN = 453,
    BYTOKEN = 454,
    DOTOKEN = 455,
    BEGINTOKEN = 456,
    ENDTOKEN = 457,
    LEFTCURLYBRACETOKEN = 458,
    RIGHTCURLYBRACETOKEN = 459,
    WHILETOKEN = 460,
    READFILETOKEN = 461,
    ISBOUNDTOKEN = 462,
    EXECUTETOKEN = 463,
    EXTERNALPROCTOKEN = 464,
    VOIDTOKEN = 465,
    CONSTANTTYPETOKEN = 466,
    FUNCTIONTOKEN = 467,
    RANGETOKEN = 468,
    INTEGERTOKEN = 469,
    STRINGTYPETOKEN = 470,
    BOOLEANTOKEN = 471,
    LISTTOKEN = 472,
    OFTOKEN = 473,
    VARTOKEN = 474,
    PROCTOKEN = 475,
    PROCEDURETOKEN = 476,
    RETURNTOKEN = 477,
    NOPTOKEN = 478,
    HELPTOKEN = 479,
    VERSIONTOKEN = 480
  };
#endif
/* Tokens.  */
#define CONSTANTTOKEN 258
#define MIDPOINTCONSTANTTOKEN 259
#define DYADICCONSTANTTOKEN 260
#define HEXCONSTANTTOKEN 261
#define HEXADECIMALCONSTANTTOKEN 262
#define BINARYCONSTANTTOKEN 263
#define PITOKEN 264
#define IDENTIFIERTOKEN 265
#define STRINGTOKEN 266
#define LPARTOKEN 267
#define RPARTOKEN 268
#define LBRACKETTOKEN 269
#define RBRACKETTOKEN 270
#define EQUALTOKEN 271
#define ASSIGNEQUALTOKEN 272
#define COMPAREEQUALTOKEN 273
#define COMMATOKEN 274
#define EXCLAMATIONTOKEN 275
#define SEMICOLONTOKEN 276
#define STARLEFTANGLETOKEN 277
#define LEFTANGLETOKEN 278
#define RIGHTANGLEUNDERSCORETOKEN 279
#define RIGHTANGLEDOTTOKEN 280
#define RIGHTANGLESTARTOKEN 281
#define RIGHTANGLETOKEN 282
#define DOTSTOKEN 283
#define DOTTOKEN 284
#define QUESTIONMARKTOKEN 285
#define VERTBARTOKEN 286
#define ATTOKEN 287
#define DOUBLECOLONTOKEN 288
#define COLONTOKEN 289
#define DOTCOLONTOKEN 290
#define COLONDOTTOKEN 291
#define EXCLAMATIONEQUALTOKEN 292
#define APPROXTOKEN 293
#define ANDTOKEN 294
#define ORTOKEN 295
#define PLUSTOKEN 296
#define MINUSTOKEN 297
#define MULTOKEN 298
#define DIVTOKEN 299
#define POWTOKEN 300
#define SQRTTOKEN 301
#define EXPTOKEN 302
#define LOGTOKEN 303
#define LOG2TOKEN 304
#define LOG10TOKEN 305
#define SINTOKEN 306
#define COSTOKEN 307
#define TANTOKEN 308
#define ASINTOKEN 309
#define ACOSTOKEN 310
#define ATANTOKEN 311
#define SINHTOKEN 312
#define COSHTOKEN 313
#define TANHTOKEN 314
#define ASINHTOKEN 315
#define ACOSHTOKEN 316
#define ATANHTOKEN 317
#define ABSTOKEN 318
#define ERFTOKEN 319
#define ERFCTOKEN 320
#define LOG1PTOKEN 321
#define EXPM1TOKEN 322
#define DOUBLETOKEN 323
#define SINGLETOKEN 324
#define HALFPRECISIONTOKEN 325
#define QUADTOKEN 326
#define DOUBLEDOUBLETOKEN 327
#define TRIPLEDOUBLETOKEN 328
#define DOUBLEEXTENDEDTOKEN 329
#define CEILTOKEN 330
#define FLOORTOKEN 331
#define NEARESTINTTOKEN 332
#define HEADTOKEN 333
#define REVERTTOKEN 334
#define SORTTOKEN 335
#define TAILTOKEN 336
#define MANTISSATOKEN 337
#define EXPONENTTOKEN 338
#define PRECISIONTOKEN 339
#define ROUNDCORRECTLYTOKEN 340
#define PRECTOKEN 341
#define POINTSTOKEN 342
#define DIAMTOKEN 343
#define DISPLAYTOKEN 344
#define VERBOSITYTOKEN 345
#define CANONICALTOKEN 346
#define AUTOSIMPLIFYTOKEN 347
#define TAYLORRECURSIONSTOKEN 348
#define TIMINGTOKEN 349
#define TIMETOKEN 350
#define FULLPARENTHESESTOKEN 351
#define MIDPOINTMODETOKEN 352
#define DIEONERRORMODETOKEN 353
#define SUPPRESSWARNINGSTOKEN 354
#define RATIONALMODETOKEN 355
#define HOPITALRECURSIONSTOKEN 356
#define ONTOKEN 357
#define OFFTOKEN 358
#define DYADICTOKEN 359
#define POWERSTOKEN 360
#define BINARYTOKEN 361
#define HEXADECIMALTOKEN 362
#define FILETOKEN 363
#define POSTSCRIPTTOKEN 364
#define POSTSCRIPTFILETOKEN 365
#define PERTURBTOKEN 366
#define MINUSWORDTOKEN 367
#define PLUSWORDTOKEN 368
#define ZEROWORDTOKEN 369
#define NEARESTTOKEN 370
#define HONORCOEFFPRECTOKEN 371
#define TRUETOKEN 372
#define FALSETOKEN 373
#define DEFAULTTOKEN 374
#define MATCHTOKEN 375
#define WITHTOKEN 376
#define ABSOLUTETOKEN 377
#define DECIMALTOKEN 378
#define RELATIVETOKEN 379
#define FIXEDTOKEN 380
#define FLOATINGTOKEN 381
#define ERRORTOKEN 382
#define QUITTOKEN 383
#define FALSEQUITTOKEN 384
#define RESTARTTOKEN 385
#define LIBRARYTOKEN 386
#define LIBRARYCONSTANTTOKEN 387
#define DIFFTOKEN 388
#define SIMPLIFYTOKEN 389
#define REMEZTOKEN 390
#define BASHEVALUATETOKEN 391
#define FPMINIMAXTOKEN 392
#define HORNERTOKEN 393
#define EXPANDTOKEN 394
#define SIMPLIFYSAFETOKEN 395
#define TAYLORTOKEN 396
#define TAYLORFORMTOKEN 397
#define AUTODIFFTOKEN 398
#define DEGREETOKEN 399
#define NUMERATORTOKEN 400
#define DENOMINATORTOKEN 401
#define SUBSTITUTETOKEN 402
#define COEFFTOKEN 403
#define SUBPOLYTOKEN 404
#define ROUNDCOEFFICIENTSTOKEN 405
#define RATIONALAPPROXTOKEN 406
#define ACCURATEINFNORMTOKEN 407
#define ROUNDTOFORMATTOKEN 408
#define EVALUATETOKEN 409
#define LENGTHTOKEN 410
#define INFTOKEN 411
#define MIDTOKEN 412
#define SUPTOKEN 413
#define MINTOKEN 414
#define MAXTOKEN 415
#define READXMLTOKEN 416
#define PARSETOKEN 417
#define PRINTTOKEN 418
#define PRINTXMLTOKEN 419
#define PLOTTOKEN 420
#define PRINTHEXATOKEN 421
#define PRINTFLOATTOKEN 422
#define PRINTBINARYTOKEN 423
#define PRINTEXPANSIONTOKEN 424
#define BASHEXECUTETOKEN 425
#define EXTERNALPLOTTOKEN 426
#define WRITETOKEN 427
#define ASCIIPLOTTOKEN 428
#define RENAMETOKEN 429
#define INFNORMTOKEN 430
#define SUPNORMTOKEN 431
#define FINDZEROSTOKEN 432
#define FPFINDZEROSTOKEN 433
#define DIRTYINFNORMTOKEN 434
#define NUMBERROOTSTOKEN 435
#define INTEGRALTOKEN 436
#define DIRTYINTEGRALTOKEN 437
#define WORSTCASETOKEN 438
#define IMPLEMENTPOLYTOKEN 439
#define IMPLEMENTCONSTTOKEN 440
#define CHECKINFNORMTOKEN 441
#define ZERODENOMINATORSTOKEN 442
#define ISEVALUABLETOKEN 443
#define SEARCHGALTOKEN 444
#define GUESSDEGREETOKEN 445
#define DIRTYFINDZEROSTOKEN 446
#define IFTOKEN 447
#define THENTOKEN 448
#define ELSETOKEN 449
#define FORTOKEN 450
#define INTOKEN 451
#define FROMTOKEN 452
#define TOTOKEN 453
#define BYTOKEN 454
#define DOTOKEN 455
#define BEGINTOKEN 456
#define ENDTOKEN 457
#define LEFTCURLYBRACETOKEN 458
#define RIGHTCURLYBRACETOKEN 459
#define WHILETOKEN 460
#define READFILETOKEN 461
#define ISBOUNDTOKEN 462
#define EXECUTETOKEN 463
#define EXTERNALPROCTOKEN 464
#define VOIDTOKEN 465
#define CONSTANTTYPETOKEN 466
#define FUNCTIONTOKEN 467
#define RANGETOKEN 468
#define INTEGERTOKEN 469
#define STRINGTYPETOKEN 470
#define BOOLEANTOKEN 471
#define LISTTOKEN 472
#define OFTOKEN 473
#define VARTOKEN 474
#define PROCTOKEN 475
#define PROCEDURETOKEN 476
#define RETURNTOKEN 477
#define NOPTOKEN 478
#define HELPTOKEN 479
#define VERSIONTOKEN 480

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 115 "/mnt/extra/ferrandi/software/panda/git-transaction-helpers/panda-framework/trunk/panda2/panda/ext/./sollya/parser.y" /* yacc.c:1909  */

  doubleNode *dblnode;
  struct entryStruct *association;
  char *value;
  node *tree;
  chain *list;
  int *integerval;
  int count;
  void *other;

#line 515 "parser.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int yyparse (void * YYLEX_PARAM);

#endif /* !YY_YY_PARSER_H_INCLUDED  */
