/* GenStg.hxx
 * Generic string formatting - with overrun protection
 *
 */

#include <stdlib.h> /* for malloc, free, etc */
#include <stdio.h>
#include <stdarg.h> /* for va_list, etc */

class GenStg {
 public:
    GenStg();
    GenStg(const char *stg);
    ~GenStg();
    const char *stg() { return m_buf; }
    void sclear(); /* clear any previous */
    const char *sprintf(const char *fmt, ...);
    const char *appendf(const char *fmt, ...);
    const char *m_appendf(const char *fmt, va_list ap); /* format using va_list */
    size_t size() { return m_strlen; }
    char * m_buf;
    size_t m_size; /* buffer size */
    size_t m_strlen; /* current length of string in above buffer */
    static const size_t m_increment = 64; /* bump for each increment */
};

extern GenStg globalStg;
#define CLEARGS globalStg.sclear()
#define LENGTHGS globalStg.size()

// eof - GenStg.hxx
