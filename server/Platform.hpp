//Platform identification tool

#ifndef PLATFORM_IDENTIFIER
#define PLATFORM_IDENTIFIER

#if __amd64__  || __x86_64__ || __amd64 || __x86_64 || _M_AMD64 || _M_X64
    #define ARCHITECTURE "x86_64"
#elif __i386__ || _M_X86 || _M_IX86
	#define ARCHITECTURE "x86"
#elif __ARM_ARCH_7__
	#define ARCHITECTURE "ARMv7"
#elif __arm__ || _M_ARM
	#define ARCHITECTURE "ARMv6"
#endif

#ifdef __linux__
	#define OPERATING_SYSTEM "Linux"
#elif _WIN32
    #define OPERATING_SYSTEM "Windows"
#elif __FreeBSD__  || __NetBSD__  || __OpenBSD__ || __DragonFly__
    #define OPERATING_SYSTEM "BSD"
#elif __APPLE__
	#define OPERATING_SYSTEM "Mac OS"
#endif

#ifndef ARCHITECTURE
#define ARCHITECTURE "Unknown"
#endif

#ifndef OPERATING_SYSTEM
#define OPERATING_SYSTEM "Unknown"
#endif

#endif
