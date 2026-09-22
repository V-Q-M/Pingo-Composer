#include "SaveDialog.h"

// The headers of the system bring their own warnings, e.g. about calls that
// Apple has retired. They are not ours to fix, so they are quiet here while
// our own code stays under the sharp settings of the build.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#pragma clang diagnostic pop

// The endings a panel offers, as the system wants them
static NSArray<UTType *> *TypesOf(const std::vector<std::string> &extensions) {
    NSMutableArray<UTType *> *types = [NSMutableArray array];

    for (const std::string &extension: extensions) {
        UTType *type = [UTType typeWithFilenameExtension:[NSString stringWithUTF8String:extension.c_str()]];

        if (type != nil) {
            [types addObject:type];
        }
    }

    return types;
}

// The save panel of macOS. It runs on the main thread and blocks until the
// user is done, which is fine: the program stands still for that moment
// anyway, like behind any other window of the system.
std::string AskWhereToSave(const std::string &suggested, const std::vector<std::string> &extensions) {
    @autoreleasepool {
        NSSavePanel *panel = [NSSavePanel savePanel];

        panel.nameFieldStringValue = [NSString stringWithUTF8String:suggested.c_str()];
        panel.canCreateDirectories = YES;

        // Only the endings the program can really write
        NSArray<UTType *> *types = TypesOf(extensions);

        if (types.count > 0) {
            panel.allowedContentTypes = types;
        }

        // The window of the program keeps the keyboard afterwards
        [NSApp activateIgnoringOtherApps:YES];

        if ([panel runModal] != NSModalResponseOK || panel.URL == nil) {
            return "";
        }

        return std::string(panel.URL.path.UTF8String);
    }
}

std::string AskWhatToOpen(const std::vector<std::string> &extensions) {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];

        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;

        NSArray<UTType *> *types = TypesOf(extensions);

        if (types.count > 0) {
            panel.allowedContentTypes = types;
        }

        [NSApp activateIgnoringOtherApps:YES];

        if ([panel runModal] != NSModalResponseOK || panel.URL == nil) {
            return "";
        }

        return std::string(panel.URL.path.UTF8String);
    }
}
