#include "SaveDialog.h"

// Outside macOS there is no panel of the system here yet: the file lands next
// to the program under the name that was suggested. The macOS version lives
// in SaveDialog.mm.
#ifndef __APPLE__

std::string AskWhereToSave(const std::string &suggested, const std::vector<std::string> &) {
    return suggested;
}

std::string AskWhatToOpen(const std::vector<std::string> &) {
    return "";
}

#endif
