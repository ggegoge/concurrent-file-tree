#ifndef _ERR_H_
#define _ERR_H_

/* print system call error message and terminate */
extern void syserr(int bl, const char* fmt, ...);

/* print error message and terminate */
extern void fatal(const char* fmt, ...);

#endif  /* _ERR_H_ */
