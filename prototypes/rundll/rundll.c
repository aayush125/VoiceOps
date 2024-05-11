#include <windows.h>
#include <winbase.h>
#include <libloaderapi.h>

typedef int (__fastcall *VOMAIN)(int, char**);

int main(int argc, char* argv[]) {
  SetDllDirectory("bin");

  // Todo: @kripesh101 - handle errors below
  HMODULE voiceops = LoadLibraryA("voiceops.dll");
  
  VOMAIN vo_main = (VOMAIN) GetProcAddress(voiceops, "main");

  return vo_main(argc, argv);
}