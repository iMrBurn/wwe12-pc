// From Ke619's SvR 2011 iOS port (github.com/Ke619/WWE-SvR11-IOS, .github/workflows/ios-port.yml).
// iOS version of plume::CocoaWindow (the macOS one in plume_apple.mm uses AppKit).
// The handle is expected to be a UIWindow*, UIView* or CALayer* (not yet tested at run time).
#include "plume_apple.h"
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <Foundation/Foundation.h>
#include <cmath>

namespace plume {
    static bool plumeIosReadSize(void* handle, CocoaWindowAttributes* out) {
        id obj = (__bridge id)handle;
        CGRect b = CGRectZero;
        CGFloat scale = 1.0;
        if ([obj isKindOfClass:[UIWindow class]]) {
            UIWindow* w = (UIWindow*)obj;
            b = w.bounds;
            scale = w.screen.nativeScale;
        } else if ([obj isKindOfClass:[UIView class]]) {
            UIView* v = (UIView*)obj;
            b = v.bounds;
            scale = v.contentScaleFactor;
        } else if ([obj isKindOfClass:[CALayer class]]) {
            CALayer* l = (CALayer*)obj;
            b = l.bounds;
            scale = l.contentsScale;
        } else {
            return false;
        }
        out->x = (int)std::lround(b.origin.x);
        out->y = (int)std::lround(b.origin.y);
        out->width = (int)std::lround(b.size.width * scale);
        out->height = (int)std::lround(b.size.height * scale);
        return true;
    }

    CocoaWindow::CocoaWindow(void* window)
        : windowHandle(window), cachedAttributes{}, cachedRefreshRate(0) {
        updateWindowAttributesInternal(false);
    }

    CocoaWindow::~CocoaWindow() {}

    void CocoaWindow::updateWindowAttributesInternal(bool forceSync) {
        auto updateBlock = ^{
            CocoaWindowAttributes a = {};
            if (plumeIosReadSize(windowHandle, &a)) {
                std::lock_guard<std::mutex> lock(attributesMutex);
                cachedAttributes = a;
            }
        };
        if (forceSync && ![NSThread isMainThread]) {
            dispatch_sync(dispatch_get_main_queue(), updateBlock);
        } else if ([NSThread isMainThread]) {
            updateBlock();
        } else {
            dispatch_async(dispatch_get_main_queue(), updateBlock);
        }
    }

    void CocoaWindow::updateRefreshRateInternal(bool forceSync) {
        auto updateBlock = ^{
            int fps = 60;
            id obj = (__bridge id)windowHandle;
            if ([obj isKindOfClass:[UIWindow class]]) {
                fps = (int)((UIWindow*)obj).screen.maximumFramesPerSecond;
            } else if ([obj isKindOfClass:[UIView class]] && ((UIView*)obj).window) {
                fps = (int)((UIView*)obj).window.screen.maximumFramesPerSecond;
            }
            cachedRefreshRate.store(fps);
        };
        if (forceSync && ![NSThread isMainThread]) {
            dispatch_sync(dispatch_get_main_queue(), updateBlock);
        } else if ([NSThread isMainThread]) {
            updateBlock();
        } else {
            dispatch_async(dispatch_get_main_queue(), updateBlock);
        }
    }

    void CocoaWindow::getWindowAttributes(CocoaWindowAttributes* attributes) const {
        const_cast<CocoaWindow*>(this)->updateWindowAttributesInternal(false);
        std::lock_guard<std::mutex> lock(attributesMutex);
        *attributes = cachedAttributes;
    }

    int CocoaWindow::getRefreshRate() const {
        const_cast<CocoaWindow*>(this)->updateRefreshRateInternal(false);
        int r = cachedRefreshRate.load();
        return r > 0 ? r : 60;
    }

    void CocoaWindow::toggleFullscreen() {}
}
