#include <csignal>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#    error "checked no-exceptions internal graph must be compiled without exception support"
#endif

#define GINT_DETAIL_CORE_ONLY
#include <gint/core.hpp>
#undef GINT_DETAIL_CORE_ONLY

#ifndef GINT_ENABLE_DIVZERO_CHECKS
#    error "checked internal graph requires GINT_ENABLE_DIVZERO_CHECKS"
#endif
#ifndef GINT_DETAIL_CORE_DEFINITIONS_INCLUDED
#    error "internal core.hpp did not complete the core definition pass"
#endif
#ifdef GINT_DETAIL_IO_DEFINITIONS_INCLUDED
#    error "internal core.hpp unexpectedly completed the IO definition pass"
#endif
#ifdef GINT_DETAIL_HEADER_PASS_ACTIVE
#    error "internal core.hpp leaked its definition-pass cleanup marker"
#endif
#ifdef GINT_DETAIL_CONFIG_NAMESPACE
#    error "internal core.hpp leaked a pass-local configuration macro"
#endif

#include <gint/gint.hpp>

#ifndef GINT_DETAIL_IO_DEFINITIONS_INCLUDED
#    error "internal gint.hpp did not complete the IO upgrade pass"
#endif
#ifdef GINT_DETAIL_HEADER_PASS_ACTIVE
#    error "internal gint.hpp leaked its definition-pass cleanup marker"
#endif
#ifdef GINT_DETAIL_CONFIG_NAMESPACE
#    error "internal gint.hpp leaked a pass-local configuration macro"
#endif

int main()
{
    const pid_t child = fork();
    if (child < 0)
        return 1;
    if (child == 0)
    {
        const rlimit no_core = {0, 0};
        static_cast<void>(setrlimit(RLIMIT_CORE, &no_core));

        const gint::UInt256 value = 1;
        const gint::UInt256 zero = 0;
        static_cast<void>(value / zero);
        _exit(0);
    }

    int status = 0;
    if (waitpid(child, &status, 0) != child)
        return 2;
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT ? 0 : 3;
}
