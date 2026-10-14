#include "../src/scriptcommands.h"
#include <cassert>
#include <iostream>

int main()
{
  assert(scriptCommandsConfigValue("\"scalance_xc208g\"") == "scalance_xc208g");
  assert(scriptCommandsConfigValue("'scalance_xr328'") == "scalance_xr328");
  assert(scriptCommandsConfigValue("\"0\"") == "0");
  assert(scriptCommandsConfigValue("'0'") == "0");
  assert(scriptCommandsConfigValue("1") == "1");
  assert(scriptCommandsConfigValue("\"scalance_xc208g'") == "\"scalance_xc208g'");
  assert(scriptCommandsLocation("", "C:/app").empty());
  assert(scriptCommandsLocation("../shared [commands]", "C:/app") == "C:/app/../shared [commands]");
#ifdef _WIN32
  assert(scriptCommandsLocation("C:\\shared commands", "D:/app") == "C:\\shared commands");
  assert(scriptCommandsLocation("\\\\server\\share", "D:/app") == "\\\\server\\share");
  assert(scriptCommandsLocation("C:relative", "D:/app").empty());
  assert(scriptCommandsLocation("\\relative", "D:/app").empty());
  assert(scriptCommandsArgument("C:\\shared commands\\") == "\"C:\\shared commands\\\\\"");
  assert(scriptCommandsArgument("with\"quote") == "\"with\\\"quote\"");
#else
  assert(scriptCommandsLocation("/shared commands", "/app") == "/shared commands");
  assert(scriptCommandsArgument("$value;$(command)") == "'$value;$(command)'");
  assert(scriptCommandsArgument("it's") == "'it'\"'\"'s'");
#endif
  std::cout << "PASS: quoted switch parameters, application-relative paths and process argument quoting\n";
}
