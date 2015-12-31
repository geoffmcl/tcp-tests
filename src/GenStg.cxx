/* GenStg.cxx
 *
 * Generic string formatting class implementaion
 *
 */

#include "GenStg.hxx"

GenStg::GenStg(): m_strlen(0)
{
    m_size = m_increment;
    m_buf = (char *)malloc(m_size * sizeof(char));
    sclear();
}

GenStg::GenStg(const char *stg): m_strlen(0)
{
    m_size = m_increment;
    m_buf = (char *)malloc(m_size * sizeof(char));
    this->sprintf("%s", stg);
}

GenStg::~GenStg()
{
    if (m_buf)
        free(m_buf);
}

void GenStg::sclear()
{
    m_strlen = 0;
    m_buf[0] = '\0';
}

const char *GenStg::sprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    sclear();
    const char *result = m_appendf(fmt, ap);
    va_end(ap);
    return result;
}

const char *GenStg::appendf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const char *result = m_appendf(fmt, ap);
    va_end(ap);
    return result;
}

const char *GenStg::m_appendf(const char *fmt, va_list ap)
{
#ifdef _MSC_VER
    int newLen;
    /* returns -1 when the string won't fit */
    while ((newLen = vsnprintf_s(m_buf + m_strlen, m_size - m_strlen, 
        m_size - m_strlen - 1, fmt, ap)) < 0) {
        m_size += m_increment;
        m_buf = (char *)realloc(m_buf, m_size);
        if (m_buf == NULL) {
            fprintf(stderr,"CRITICAL ERROR: Memory reallocation to %d bytes FAILED!\n", m_size);
            exit(1);
        }
    }
#else
    size_t newLen;
    va_list ap_copy;
    va_copy(ap_copy, ap);
    newLen = vsnprintf(m_buf + m_strlen, m_size - m_strlen, fmt, ap);
    if ((newLen + 1) > (m_size - m_strlen)) {
        m_size = (m_strlen + newLen + 1 + m_increment) / m_increment * m_increment;
        m_buf = (char *)realloc(m_buf, m_size);
        if (m_buf == NULL) {
            fprintf(stderr,"CRITICAL ERROR: Memory reallocation to %d bytes FAILED!\n", m_size);
            exit(1);
        }
    	vsnprintf(m_buf + m_strlen, m_size - m_strlen, fmt, ap_copy);
    }
    va_end(ap_copy);
#endif
    m_strlen += newLen; /* update buffer length */
    return m_buf;
}

GenStg globalStg; /* for global use */

/* eof - GenStg.cxx */

