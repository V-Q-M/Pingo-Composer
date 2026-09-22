#pragma once

#include <string>
#include <vector>

// Asks the system where a file should be saved.
//
// The panel of the operating system is used instead of a window of our own:
// it knows the folders of the user, iCloud and everything else, and it looks
// like every other program. Outside macOS the file simply lands next to the
// program, so nothing has to be built twice.
//
// suggested is the name the panel starts with, e.g. "song.wav". extensions are
// the endings that may be chosen, e.g. {"wav", "mid"}. The answer is the whole
// path, empty when the panel was cancelled.
std::string AskWhereToSave(const std::string &suggested, const std::vector<std::string> &extensions);
