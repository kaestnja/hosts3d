#ifndef HOSTS3D_SCRIPTCOMMANDS_H
#define HOSTS3D_SCRIPTCOMMANDS_H

#include <cctype>
#include <string>

// Resolve against the executable directory, independent of the launch directory.
inline std::string scriptCommandsLocation(const std::string &configured, const std::string &applicationDir)
{
  if (configured.empty() || applicationDir.empty()) return "";
#ifdef _WIN32
  bool drive = configured.size() > 1 && std::isalpha(static_cast<unsigned char>(configured[0])) && configured[1] == ':';
  bool rooted = configured[0] == '/' || configured[0] == '\\';
  bool unc = configured.size() > 1 && rooted && (configured[1] == '/' || configured[1] == '\\');
  if (drive && (configured.size() < 3 || (configured[2] != '/' && configured[2] != '\\'))) return "";
  if (rooted && !unc) return "";
  if (drive || unc) return configured;
#else
  if (configured[0] == '/') return configured;
#endif
  return applicationDir + "/" + configured;
}

// Quote one process argument; Windows uses CreateProcess, POSIX uses /bin/sh.
inline std::string scriptCommandsArgument(const std::string &value)
{
#ifdef _WIN32
  std::string result = "\"";
  size_t slashes = 0;
  for (char ch : value)
  {
    if (ch == '\\') { ++slashes; continue; }
    result.append(slashes * (ch == '"' ? 2 : 1), '\\');
    slashes = 0;
    if (ch == '"') result += '\\';
    result += ch;
  }
  result.append(slashes * 2, '\\');
  return result + '"';
#else
  std::string result = "'";
  for (char ch : value) result += ch == '\'' ? "'\"'\"'" : std::string(1, ch);
  return result + "'";
#endif
}

#endif
