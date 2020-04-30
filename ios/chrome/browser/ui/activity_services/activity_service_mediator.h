// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace bookmarks {
class BookmarkModel;
}

@protocol BrowserCommands;
@protocol ChromeActivityItemSource;
@protocol FindInPageCommands;
class PrefService;
@protocol QRGenerationCommands;
@class ShareToData;

// Snackbar ID for any services that wish to show snackbars.
extern NSString* const kActivityServicesSnackbarCategory;

// Mediator used to generate activities.
@interface ActivityServiceMediator : NSObject

// Initializes a mediator instance with a |handler| used to execute action, a
// |prefService| to read settings and policies, and a |bookmarkModel| to
// retrieve bookmark states.
- (instancetype)
    initWithHandler:
        (id<BrowserCommands, FindInPageCommands, QRGenerationCommands>)handler
        prefService:(PrefService*)prefService
      bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Generates an array of activity items to be shared via an activity view for
// the given |data|.
- (NSArray*)activityItemsForData:(ShareToData*)data;

// Generates an array of activities to be added to the activity view for the
// given |data|.
- (NSArray*)applicationActivitiesForData:(ShareToData*)data;

// Returns the union of excluded activity types given |items| to share.
- (NSSet*)excludedActivityTypesForItems:
    (NSArray<id<ChromeActivityItemSource>>*)items;

// Handles completion of a share action.
- (void)shareFinishedWithActivityType:(NSString*)activityType
                            completed:(BOOL)completed
                        returnedItems:(NSArray*)returnedItems
                                error:(NSError*)activityError;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_MEDIATOR_H_
