#ifndef _ERR_H_
#define _ERR_H_

/** Print system call error message and terminate. If `e` is zero do nothing. */
extern void syserr(int e, const char* fmt, ...);

/** Print error message and terminate. */
extern void fatal(const char* fmt, ...);

#endif  /* _ERR_H_ */
