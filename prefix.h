/*
   prefix.h

   eJS Project
     Kochi University of Technology
     the University of Electro-communications

     Tomoharu Ugawa, 2016-17
     Hideya Iwasaki, 2016-17

   The eJS Project is the successor of the SSJS Project at the University of
   Electro-communications, which was contributed by the following members.

     Sho Takada, 2012-13
     Akira Tanimura, 2012-13
     Akihiro Urushihara, 2013-14
     Ryota Fujii, 2013-14
     Tomoharu Ugawa, 2012-14
     Hideya Iwasaki, 2012-14
*/

#ifndef PREFIX_H_
#define PREFIX_H_

/*
   compilation options
 */

#ifndef NDEBUG
#define DEBUG 1
#define DEBUG_PRINT
#endif

#define STROBJ_HAS_HASH

#define USE_THRESHOLD
//#define CALC_TIME
//#define USE_PAPI
//#define USE_FASTGLOBAL
//#define USE_ASM2
//#define CALC_CALL

#define HIDDEN_CLASS

#if 0
#ifdef DEBUG_PRINT

#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define LOG_FUNC fprintf(stderr, "%-16s: ", __func__)
#define LOG_ERR(...) do { LOG_FUNC; fprintf(stderr, __VA_ARGS__); } while (0)
#define LOG_EXIT(...) do { LOG_FUNC; fprintf(stderr, __VA_ARGS__); exit(1); } while (0)

#else
#define LOG
#define LOG_FUNC
#define LOG_ERR
#define LOG_EXIT(...) exit(1)

#endif // DEBUG
#endif

#ifdef CALC_CALL
#define CALLCOUNT_UP() callcount++
#else
#define CALLCOUNT_UP()
#endif

#endif // PREFIX_H_
