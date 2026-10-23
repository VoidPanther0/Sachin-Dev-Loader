#pragma once

#include <string>

namespace dialogs {

// Native "Open File" picker. filter is the Win32 double-NUL-terminated
// filter string, e.g. "Executables (*.exe)\0*.exe\0All files (*.*)\0*.*\0".
// Returns false if the user cancelled.
bool open_file(const char* filter, const char* title, std::string* out_path);

// Native "Save File" picker. default_ext is added automatically if the user
// does not type an extension (no leading dot, e.g. "txt").
bool save_file(const char* filter, const char* title, const char* default_ext, std::string* out_path);

// Native folder picker.
bool pick_folder(const char* title, std::string* out_path);

} // namespace dialogs
