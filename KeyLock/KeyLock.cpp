#include "Application.h"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shlwapi.lib")

int APIENTRY _tWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE previous_instance, _In_ LPTSTR command_line, _In_ int cmd_show) {
  UNREFERENCED_PARAMETER(previous_instance);
  UNREFERENCED_PARAMETER(command_line);

  Application app(instance);
  return app.Run();
}
