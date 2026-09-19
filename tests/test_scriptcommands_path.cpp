#include "../src/scriptcommands.h"
#include <cassert>
#include <iostream>

int main()
{
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
  std::cout << "PASS: application-relative checkout paths and process argument quoting\n";
}
