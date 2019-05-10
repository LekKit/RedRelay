//Cross-platform console colors

#ifdef _WIN32

#include <windows.h>
HANDLE ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

#else

std::string ConsoleColors[]={"\033[0;30m",
                             "\033[0;34m",
                             "\033[0;32m",
                             "\033[0;36m",
                             "\033[0;31m",
                             "\035[0;31m",
                             "\033[0;33m",
                             "\033[0;37m",
                             "\033[1;30m",
                             "\033[1;34m",
                             "\033[1;32m",
                             "\033[1;36m",
                             "\033[1;31m",
                             "\035[1;31m",
                             "\033[1;33m",
                             "\033[1;37m"};

#endif

inline void ChangeConsoleColor(uint8_t color){
#ifdef _WIN32
	SetConsoleTextAttribute(ConsoleHandle, color);
#else
	std::cout<<ConsoleColors[color];
#endif
}
