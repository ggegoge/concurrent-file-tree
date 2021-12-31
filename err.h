#ifndef _ERR_H_
#define _ERR_H_

/** Print system call error message and terminate. If `e` is zero do nothing. */
extern void syserr(int e, const char* fmt, ...);

#endif  /* _ERR_H_ */
