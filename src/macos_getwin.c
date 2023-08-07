// MaCoPiX : Mascot Constructive Pilot for X
//    macos_getwin.c
//       Get Window Info in macOS
//                                                     2019.6 K.Chimari

#include "main.h"

#ifdef USE_OSX
#include <gdk/gdkquartz.h>
#include <gdk/quartz/gdkquartz-cocoa-access.h>

#include <Carbon/Carbon.h>
#import <objc/Object.h>
#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>

CFDictionaryRef bounds = NULL;

void MacMapWin(GtkWidget *w, gboolean f){
  NSWindow* nswin = gdk_quartz_window_get_nswindow(gtk_widget_get_window(w));

  if(f){
    [nswin orderFrontRegardless:nswin];
  }
  else{
    [nswin orderOut:nswin];
  }
}

void MacSetWidgetLayer(GtkWidget *w, gboolean top_flag){
  NSWindow *nswin = gdk_quartz_window_get_nswindow(gtk_widget_get_window(w));

  if(top_flag){
    [nswin setLevel: NSMainMenuWindowLevel+1];
  }
  else{
    [nswin setLevel: NSFloatingWindowLevel];
  }
    [nswin setHasShadow:NO];
    [nswin setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary | NSWindowCollectionBehaviorTransient | NSWindowCollectionBehaviorIgnoresCycle];
}


void MacSetLayer(typMascot *mascot){
  // Main 
  switch(mascot->mac_layer){
  case MAC_LAYER_TOP:
    MacSetWidgetLayer(mascot->win_main, TRUE);
    MacSetWidgetLayer(mascot->balloon_main, TRUE);
    MacSetWidgetLayer(mascot->clock_main, TRUE);
    MacSetWidgetLayer(mascot->biff_pix, TRUE);
    break;

  default:
    MacSetWidgetLayer(mascot->win_main, FALSE);
    MacSetWidgetLayer(mascot->balloon_main, FALSE);
    MacSetWidgetLayer(mascot->clock_main, FALSE);
    MacSetWidgetLayer(mascot->biff_pix, FALSE);
    break;
  }
}

// for debug
void MacPrintCurrentWindows(){
  int x,y,w,h;

  CFArrayRef windowArray = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
  NSMutableArray *windowsInMap = [NSMutableArray arrayWithCapacity:64];
  NSArray*  windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
  NSUInteger count = [windows count];
  
  for (NSUInteger i = 0; i < count; i++){
    NSDictionary*   nswindowsdescription = [windows objectAtIndex:i];
    NSNumber* windowid = (NSNumber*)[nswindowsdescription objectForKey:@"kCGWindowNumber"];
    NSString* windowName = (NSString*)[nswindowsdescription objectForKey:@"kCGWindowOwnerName"];
    CGRect rect;
    CFDictionaryRef windowdescription = (CFDictionaryRef)CFArrayGetValueAtIndex ((CFArrayRef)windowArray, i);
    
    
    if(windowid){
      int layer;
      CFNumberGetValue(CFDictionaryGetValue(windowdescription, kCGWindowLayer), kCFNumberIntType, &layer);
      if(layer==0){
	if(bounds) CFRelease(bounds);
	bounds = (CFDictionaryRef)CFDictionaryGetValue (windowdescription, kCGWindowBounds);
	if(bounds){
	  CGRectMakeWithDictionaryRepresentation(bounds, &rect);
	  x=(int)rect.origin.x;
	  y=(int)rect.origin.y;
	  w=(int)rect.size.width;
	  h=(int)rect.size.height;
	  printf("%s:%s  (%d, %d)  %dx%d   @%d\n", [windowName UTF8String], [[windowid stringValue] UTF8String], x,y,w,h,layer);
	}
      }
    }
    
  }
  CFRelease(windowArray);
  [windows release];
}


void MacGetFocusWin(int *x, int *y, int *w, int *h){
  gboolean win_flag=FALSE;
  *x=-1;
  *y=-1;
  *w=-1;
  *h=-1;

  CFArrayRef windowArray = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
  //NSArray*  windows = (NSArray*)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
  //NSArray*  windows = (NSArray*)windowArray;
  NSArray* windows = CFBridgingRelease(windowArray);
  NSUInteger count = [windows count];
  static CGRect rect;
 
  for (NSUInteger i = 0; i < count; i++){
    NSDictionary*   nswindowsdescription = [windows objectAtIndex:i];
    NSNumber* windowid = (NSNumber*)[nswindowsdescription objectForKey:@"kCGWindowNumber"];
    NSString* windowName = (NSString*)[nswindowsdescription objectForKey:@"kCGWindowOwnerName"];
    CFDictionaryRef windowdescription = (CFDictionaryRef)CFArrayGetValueAtIndex ((CFArrayRef)windowArray, i);

    if(windowid){
      if(!windowName){
	win_flag=TRUE;
      }
      else if(strncasecmp([windowName UTF8String], "macopix", strlen("macopix"))!=0){
	win_flag=TRUE;
      }else{
	win_flag=FALSE;
      }

      if(win_flag){
	int layer;
	CFNumberGetValue(CFDictionaryGetValue(windowdescription, kCGWindowLayer), kCFNumberIntType, &layer);
	if(layer==0){
	  CFDictionaryRef bounds = (CFDictionaryRef)CFDictionaryGetValue (windowdescription, kCGWindowBounds);
	  if(bounds){
	    CGRectMakeWithDictionaryRepresentation(bounds, &rect);
	    *x=(int)rect.origin.x;
	    *y=(int)rect.origin.y;
	    *w=(int)rect.size.width;
	    *h=(int)rect.size.height;
	    if((x!=0)||(y!=0)){
	      //printf("%s:%s\n", [windowName UTF8String], [[windowid stringValue] UTF8String]);
	      //CFRelease(nswindowsdescription);
	      break;
	    }
	    CFRelease(bounds);
	  }
	}
	//CFRelease(nswindowsdescription);
      }
    }
  }
  //[windows release];
  //CFRelease(windowArray);
}

void MacGetRootWin(int *w, int *h){
  *w=-1;
  *h=-1;
  
  // convert screen coordinates to global display coordinates
  NSRect r = [[[NSScreen screens] objectAtIndex: 0] frame];

  *w=(int)r.size.width;
  *h=(int)r.size.height;
}

/*
int main(int argc, char **argv) {
  int root_w, root_h;
  int x, y, w, h;
  sleep(5);

  MacGetRootWin(&root_w, &root_h);
  printf(" Root =   %dx%d\n",
	 root_w, root_h);
  
  MacGetFocusWin(&x, &y, &w, &h);
  printf("  (%d, %d) %dx%d\n",
	 x, y, w, h);
}
*/

#endif
