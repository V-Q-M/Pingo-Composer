#include "SaveDialog.h"

// The macOS version lives in SaveDialog.mm. On Linux the panel of the desktop
// is asked through zenity or kdialog, whichever is installed. Everywhere else
// the file lands next to the program under the name that was suggested.
#ifndef __APPLE__

#ifdef __linux__

#include <cstdio>
#include <cstdlib>

namespace {

bool Exists(const std::string &program) {
    std::string command = "command -v " + program + " >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
}

// Quotes a text for the shell
std::string Quote(const std::string &text) {
    std::string quoted = "'";
    for (char c : text) {
        quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
    }
    return quoted + "'";
}

// Runs the dialog and returns the first line it printed, empty when cancelled
std::string Run(const std::string &command) {
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }

    std::string answer;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        answer += buffer;
    }

    if (pclose(pipe) != 0) {
        return "";
    }

    while (!answer.empty() && (answer.back() == '\n' || answer.back() == '\r')) {
        answer.pop_back();
    }
    return answer;
}

// Both programs take a filter like "*.wav *.mid"
std::string Filter(const std::vector<std::string> &extensions) {
    std::string filter;
    for (const std::string &extension : extensions) {
        filter += (filter.empty() ? "*." : " *.") + extension;
    }
    return filter;
}

std::string Ask(bool save, const std::string &suggested, const std::vector<std::string> &extensions) {
    std::string filter = Filter(extensions);

    if (Exists("zenity")) {
        std::string command = "zenity --file-selection";
        if (save) {
            command += " --save --confirm-overwrite --filename=" + Quote(suggested);
        }
        if (!filter.empty()) {
            command += " --file-filter=" + Quote(filter);
        }
        return Run(command + " 2>/dev/null");
    }

    if (Exists("kdialog")) {
        std::string command = save ? "kdialog --getsavefilename " + Quote(suggested) : "kdialog --getopenfilename .";
        if (!filter.empty()) {
            command += " " + Quote(filter);
        }
        return Run(command + " 2>/dev/null");
    }

    return save ? suggested : "";
}

} // namespace

std::string AskWhereToSave(const std::string &suggested, const std::vector<std::string> &extensions) {
    return Ask(true, suggested, extensions);
}

std::string AskWhatToOpen(const std::vector<std::string> &extensions) {
    return Ask(false, "", extensions);
}

#else

std::string AskWhereToSave(const std::string &suggested, const std::vector<std::string> &) {
    return suggested;
}

std::string AskWhatToOpen(const std::vector<std::string> &) {
    return "";
}

#endif

#endif
