#ifndef LOGGINGASSERT_H
#define LOGGINGASSERT_H

#define ASSERT_HALT() (std::abort())

#define ASSERT_HANDLER(x, y, z, t) (Lichtenstein::Server::Logging::assertFailed)(x, y, z, t)
#define XASSERT(x, m) (!(x) && ASSERT_HANDLER(#x, __FILE__, __LINE__, m) && (ASSERT_HALT(), 1))

#endif
