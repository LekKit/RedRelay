//Platform identification tool

#ifndef PLATFORM_IDENTIFIER
#define PLATFORM_IDENTIFIER

#if defined(__amd64__)  || defined(__x86_64__) || defined(__amd64) || defined(__x86_64) || defined(_M_AMD64) || defined(_M_X64)
    #define ARCHITECTURE "x86_64"
#elif defined(__i386__) || defined(_M_X86) || defined(_M_IX86)
	#define ARCHITECTURE "x86"
#elif defined(__ARM_ARCH_7__)
	#define ARCHITECTURE "ARMv7"
#elif defined(__arm__) || defined(_M_ARM)
	#define ARCHITECTURE "ARMv6"
#endif

#if defined(__linux__)
	#define OPERATING_SYSTEM "Linux"
#elif defined(_WIN32)
    #define OPERATING_SYSTEM "Windows"
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    #define OPERATING_SYSTEM "BSD"
#elif defined(__APPLE__)
	#define OPERATING_SYSTEM "Mac OS"
#endif

#ifndef ARCHITECTURE
#define ARCHITECTURE "Unknown"
#endif

#ifndef OPERATING_SYSTEM
#define OPERATING_SYSTEM "Unknown"
#endif

#endif
