#ifndef LOG_H
#define LOG_H

#ifdef _DEBUG_
#define log _log
#else
#define log(format, ...)
#endif

#endif