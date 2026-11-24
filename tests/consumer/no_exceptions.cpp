#include <gint/gint.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <limits>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#    error "this consumer must be compiled with exceptions disabled"
#endif
#if defined(GINT_DETAIL_CONFIG_NAMESPACE) || defined(GINT_FORCE_INLINE) || defined(GINT_THROW) || defined(GINT_DETAIL_EXCEPTIONS_ENABLED) \
    || defined(GINT_DETAIL_DIVZERO_CHECKS)
#    error "complete header leaked a private macro"
#endif

namespace
{
template <typename Fn>
bool expect_sigabrt(const char * operation, Fn fn)
{
    const pid_t child = fork();
    if (child == -1)
    {
        std::perror("fork");
        return false;
    }
    if (child == 0)
    {
        const struct rlimit core_limit = {0, 0};
        if (setrlimit(RLIMIT_CORE, &core_limit) != 0)
        {
            std::perror("setrlimit");
            _exit(2);
        }
        fn();
        _exit(0);
    }

    int status = 0;
    pid_t waited;
    do
    {
        waited = waitpid(child, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1)
    {
        std::perror("waitpid");
        return false;
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT)
        return true;

    if (WIFSIGNALED(status))
        std::fprintf(stderr, "%s: expected SIGABRT, got signal %d\n", operation, WTERMSIG(status));
    else if (WIFEXITED(status))
        std::fprintf(stderr, "%s: expected SIGABRT, exited with status %d\n", operation, WEXITSTATUS(status));
    else
        std::fprintf(stderr, "%s: expected SIGABRT, got wait status %d\n", operation, status);
    return false;
}
}

int main()
{
    using gint::Int256;
    using gint::UInt256;
    const UInt256 lhs = (UInt256(1) << 200) + 123;
    const UInt256 rhs = 7;
    const UInt256 quotient = lhs / rhs;
    const UInt256 remainder = lhs % rhs;
    const auto qr = gint::divmod(lhs, rhs);
    bool ok = quotient * rhs + remainder == lhs && remainder < rhs && qr.quotient == quotient && qr.remainder == remainder;
    if (!ok)
        std::fputs("normal division, modulo and divmod disagree\n", stderr);

    ok &= expect_sigabrt("empty string", [] { gint::from_string<UInt256>("", 10); });
    ok &= expect_sigabrt("sign without digits", [] { gint::from_string<UInt256>("+", 10); });
    ok &= expect_sigabrt("prefix without digits", [] { gint::from_string<UInt256>("0x", 16); });
    ok &= expect_sigabrt("invalid digit", [] { gint::from_string<UInt256>("12g3", 10); });
    ok &= expect_sigabrt("invalid base", [] { gint::from_string<UInt256>("123", 1); });
    ok &= expect_sigabrt("null string", [] { gint::from_string<UInt256>(static_cast<const char *>(nullptr), 10); });
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    ok &= expect_sigabrt("integer / NaN", [nan] { static_cast<void>(UInt256(7) / nan); });
    ok &= expect_sigabrt("integer % NaN", [nan] { static_cast<void>(UInt256(7) % nan); });
    ok &= expect_sigabrt("NaN / integer", [nan] { static_cast<void>(nan / Int256(3)); });
    ok &= expect_sigabrt("NaN % integer", [nan] { static_cast<void>(nan % Int256(3)); });
    ok &= expect_sigabrt("+inf / integer", [inf] { static_cast<void>(inf / Int256(3)); });
    ok &= expect_sigabrt("-inf % integer", [inf] { static_cast<void>(-inf % Int256(3)); });

#ifdef GINT_ENABLE_DIVZERO_CHECKS
    ok &= expect_sigabrt("integer / zero", [] { static_cast<void>(UInt256(1) / UInt256(0)); });
    ok &= expect_sigabrt("integer % zero", [] { static_cast<void>(UInt256(1) % UInt256(0)); });
    ok &= expect_sigabrt("divmod by zero", [] { static_cast<void>(gint::divmod(UInt256(1), UInt256(0))); });
    ok &= expect_sigabrt("integer / 0.5", [] { static_cast<void>(UInt256(5) / 0.5); });
    ok &= expect_sigabrt("integer % 0.5", [] { static_cast<void>(UInt256(5) % 0.5); });
#else
    const UInt256 value = 42;
    const UInt256 zero = 0;
    const auto zero_qr = gint::divmod(value, zero);
    if (value / zero != zero || value % zero != value || zero_qr.quotient != zero || zero_qr.remainder != value || value / 0.5 != zero
        || value % 0.5 != value)
    {
        std::fputs("unchecked zero divisor must return quotient 0 and the original remainder\n", stderr);
        ok = false;
    }
#endif
    return ok ? 0 : 1;
}
