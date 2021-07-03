/*
 
 File: script.js
 
 Abstract: Event handling code for the TouchEventDemo sample.
 
 Version: 1.0
 
 Disclaimer: IMPORTANT:  This Apple software is supplied to you by 
 Apple Inc. ("Apple") in consideration of your agreement to the
 following terms, and your use, installation, modification or
 redistribution of this Apple software constitutes acceptance of these
 terms.  If you do not agree with these terms, please do not use,
 install, modify or redistribute this Apple software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive
 license, under Apple's copyrights in this original Apple software (the
 "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following
 text and disclaimers in all such redistributions of the Apple Software. 
 Neither the name, trademarks, service marks or logos of Apple Inc. 
 may be used to endorse or promote products derived from the Apple
 Software without specific prior written permission from Apple.  Except
 as expressly stated in this notice, no other rights or licenses, express
 or implied, are granted by Apple herein, including but not limited to
 any patent rights that may be infringed by your derivative works or by
 other works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
 MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
 THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
 OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
 AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
 STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 
 Copyright (C) 2010 Apple Inc. All Rights Reserved.
 
*/

const MIN_TOUCHMOVE_DISTANCE_SQUARED = 49;

function TouchEventDemoController() {
  this.startPositions = new Object();
  
  // Don't allow the document to pan on iPhone OS
  document.addEventListener('touchmove', function(e) { e.preventDefault(); }, false);
}

TouchEventDemoController.prototype.distanceSquared = function(x1, y1, x2, y2) {
  return ((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

// Sets and then quickly removes a 'highlighted' class an element. 
// CSS Transitions will handle the actual animation.
TouchEventDemoController.prototype.flashEvent = function(id) {
  var element = document.getElementById(id);
  element.className = 'highlighted';
  window.setTimeout(function() { 
    element.className = ''; 
  }, 0);
}

TouchEventDemoController.prototype.registerHandler = function(eventName) {
  // If we pass in an object instead of a method as addEventListener's second parameter,
  // when the event is dispatched a method named 'handleEvent' will be called on the object.
  // 
  // Mosue events are not dispatched to the document node, so use a hidden element that
  // overlays the whole page instead.
  document.getElementById('eventtarget').addEventListener(eventName, this, false);
}

TouchEventDemoController.prototype.handleEvent = function(event) {
  this[event.type](event);
}

/******************************************************************************
  Event Handlers 
******************************************************************************/

TouchEventDemoController.prototype.mousedown = function(event) {
  this.flashEvent('mousedown');
}

TouchEventDemoController.prototype.mousemove = function(event) {
  this.flashEvent('mousemove');
}

TouchEventDemoController.prototype.mouseup = function(event) {
  this.flashEvent('mouseup');
}

TouchEventDemoController.prototype.touchstart = function(event) {
  for (var i = 0; i < event.changedTouches.length; i++) {
    var touch = event.changedTouches[i];
    this.startPositions[touch.identifier] = { x: touch.pageX, y: touch.pageY };
  }
  
  this.flashEvent('touchstart');
}

TouchEventDemoController.prototype.touchmove = function(event) {
  // To make demonstrations easier, suppress touchmove events until a finger has moved more than
  // a few pixels away from where the touch started.
  for (var i = 0; i < event.changedTouches.length; i++) {
    var touch = event.changedTouches[i];
    startPosition = this.startPositions[touch.identifier]
    if (startPosition && this.distanceSquared(touch.pageX, touch.pageY, startPosition.x, startPosition.y) < MIN_TOUCHMOVE_DISTANCE_SQUARED)
      continue;
    
    delete this.startPositions[touch.identifier];
    this.flashEvent('touchmove');
  }
}

TouchEventDemoController.prototype.touchend = function(event) {
  for (var i = 0; i < event.changedTouches.length; i++)
    delete this.startPositions[event.changedTouches[i].identifier];
  
  this.flashEvent('touchend');
}

TouchEventDemoController.prototype.touchcancel = function(event) {
  this.startPositions = new Object();
  this.flashEvent('touchcancel');
}

TouchEventDemoController.prototype.gesturestart = function(event) {
  this.flashEvent('gesturestart');
}

TouchEventDemoController.prototype.gesturechange = function(event) {
  this.flashEvent('gesturechange');
}

TouchEventDemoController.prototype.gestureend = function(event) {
  this.flashEvent('gestureend');
}
