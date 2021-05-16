/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AnimationController.h"
#include "AnimationControllerPrivate.h"
#include "AnimationBase.h"
#include "CompositeAnimation.h"
#include "CSSParser.h"
#include "EventNames.h"
#include "Frame.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

static const double cAnimationTimerDelay = 0.025;
static const double cBeginAnimationUpdateTimeNotSet = -1;

AnimationControllerPrivate::AnimationControllerPrivate(Frame* frame)
    : m_animationTimer(this, &AnimationControllerPrivate::animationTimerFired)
    , m_updateRenderingDispatcher(this, &AnimationControllerPrivate::updateRenderingDispatcherFired)
    , m_frame(frame)
    , m_beginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet)
    , m_styleAvailableWaiters(0)
    , m_lastStyleAvailableWaiter(0)
    , m_responseWaiters(0)
    , m_lastResponseWaiter(0)
    , m_waitingForAResponse(false)
{
}

AnimationControllerPrivate::~AnimationControllerPrivate()
{
}

PassRefPtr<CompositeAnimation> AnimationControllerPrivate::accessCompositeAnimation(RenderObject* renderer)
{
    RefPtr<CompositeAnimation> animation = m_compositeAnimations.get(renderer);
    if (!animation) {
        animation = CompositeAnimation::create(this);
        m_compositeAnimations.set(renderer, animation);
    }
    return animation;
}

bool AnimationControllerPrivate::clear(RenderObject* renderer)
{
    // Return false if we didn't do anything OR we are suspended (so we don't try to
    // do a setChanged() when suspended).
    PassRefPtr<CompositeAnimation> animation = m_compositeAnimations.take(renderer);
    if (!animation)
        return false;
    animation->clearRenderer();
    return animation->isSuspended();
}

void AnimationControllerPrivate::updateAnimationTimer(bool callSetChanged/* = false*/)
{
    double needsService = -1;
    bool calledSetChanged = false;

    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        CompositeAnimation* compAnim = it->second.get();
        if (!compAnim->isSuspended() && compAnim->hasAnimations()) {
            double t = compAnim->timeToNextService();
            if (t != -1 && (t < needsService || needsService == -1))
                needsService = t;
            if (needsService == 0) {
                if (callSetChanged) {
                    Node* node = it->first->element();
                    ASSERT(!node || (node->document() && !node->document()->inPageCache()));
                    node->setChanged(AnimationStyleChange);
                    calledSetChanged = true;
                }
                else
                    break;
            }
        }
    }
    
    if (calledSetChanged)
        m_frame->document()->updateRendering();
    
    // If we want service immediately, we start a repeating timer to reduce the overhead of starting
    if (needsService == 0) {
        if (!m_animationTimer.isActive() || m_animationTimer.repeatInterval() == 0)
            m_animationTimer.startRepeating(cAnimationTimerDelay);
        return;
    }
    
    // If we don't need service, we want to make sure the timer is no longer running
    if (needsService < 0) {
        if (m_animationTimer.isActive())
            m_animationTimer.stop();
        return;
    }
    
    // Otherwise, we want to start a one-shot timer so we get here again
    if (m_animationTimer.isActive())
        m_animationTimer.stop();
    m_animationTimer.startOneShot(needsService);
}

void AnimationControllerPrivate::updateRenderingDispatcherFired(Timer<AnimationControllerPrivate>*)
{
    // fire all the events
    Vector<EventToDispatch>::const_iterator eventsToDispatchEnd = m_eventsToDispatch.end();
    for (Vector<EventToDispatch>::const_iterator it = m_eventsToDispatch.begin(); it != eventsToDispatchEnd; ++it) {
        if (it->eventType == eventNames().webkitTransitionEndEvent)
            it->element->dispatchWebKitTransitionEvent(it->eventType,it->name, it->elapsedTime);
        else
            it->element->dispatchWebKitAnimationEvent(it->eventType,it->name, it->elapsedTime);
    }
    
    m_eventsToDispatch.clear();
    
    // call setChanged on all the elements
    Vector<RefPtr<Node> >::const_iterator nodeChangesToDispatchEnd = m_nodeChangesToDispatch.end();
    for (Vector<RefPtr<Node> >::const_iterator it = m_nodeChangesToDispatch.begin(); it != nodeChangesToDispatchEnd; ++it)
        (*it)->setChanged(AnimationStyleChange);
    
    m_nodeChangesToDispatch.clear();
    
    if (m_frame && m_frame->document())
        m_frame->document()->updateRendering();

    // We can now safely remove any animations or transitions that are finished.
    // We can't remove them any earlier because we might get a false restart of
    // a transition. This can happen because we have not yet set the final property
    // value until we call the rendering dispatcher. So this can make the current
    // style slightly different from the desired final style (because our last 
    // animation step was, say 0.9999 or something). And we need to remove them
    // here because if there are no more animations running we'll never get back
    // into the animation code to clean them up.
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        CompositeAnimation* compAnim = it->second.get();
        compAnim->cleanupFinishedAnimations(); // will not modify m_compositeAnimations, so OK to call while iterating
    }
}

void AnimationControllerPrivate::startUpdateRenderingDispatcher()
{
    if (!m_updateRenderingDispatcher.isActive())
        m_updateRenderingDispatcher.startOneShot(0);
}

void AnimationControllerPrivate::addEventToDispatch(PassRefPtr<Element> element, const AtomicString& eventType, const String& name, double elapsedTime)
{
    m_eventsToDispatch.grow(m_eventsToDispatch.size()+1);
    EventToDispatch& event = m_eventsToDispatch[m_eventsToDispatch.size()-1];
    event.element = element;
    event.eventType = eventType;
    event.name = name;
    event.elapsedTime = elapsedTime;
    
    startUpdateRenderingDispatcher();
}

void AnimationControllerPrivate::addNodeChangeToDispatch(PassRefPtr<Node> node)
{
    ASSERT(!node || (node->document() && !node->document()->inPageCache()));
    if (!node)
        return;

    m_nodeChangesToDispatch.append(node);
    startUpdateRenderingDispatcher();
}

void AnimationControllerPrivate::animationTimerFired(Timer<AnimationControllerPrivate>*)
{
    // Make sure animationUpdateTime is updated, so that it is current even if no
    // styleChange has happened (e.g. accelerated animations)
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);

    // When the timer fires, all we do is call setChanged on all DOM nodes with running animations and then do an immediate
    // updateRendering.  It will then call back to us with new information.
    updateAnimationTimer(true);
}

bool AnimationControllerPrivate::isAnimatingPropertyOnRenderer(RenderObject* renderer, CSSPropertyID property, bool isRunningNow) const
{
    RefPtr<CompositeAnimation> animation = m_compositeAnimations.get(renderer);
    if (!animation)
        return false;

    return animation->isAnimatingProperty(property, isRunningNow);
}

void AnimationControllerPrivate::suspendAnimations(Document* document)
{
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);
    
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        RenderObject* renderer = it->first;
        if (renderer->document() == document) {
            CompositeAnimation* compAnim = it->second.get();
            compAnim->suspendAnimations();
        }
    }
    
    updateAnimationTimer();
}

void AnimationControllerPrivate::resumeAnimations(Document* document)
{
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);
    
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        RenderObject* renderer = it->first;
        if (renderer->document() == document) {
            CompositeAnimation* compAnim = it->second.get();
            compAnim->resumeAnimations();
        }
    }
    
    updateAnimationTimer();
}

bool AnimationControllerPrivate::pauseAnimationAtTime(RenderObject* renderer, const String& name, double t)
{
    if (!renderer)
        return false;

    RefPtr<CompositeAnimation> compAnim = accessCompositeAnimation(renderer);
    if (!compAnim)
        return false;

    if (compAnim->pauseAnimationAtTime(name, t)) {
        renderer->node()->setChanged(AnimationStyleChange);
        startUpdateRenderingDispatcher();
        return true;
    }

    return false;
}

bool AnimationControllerPrivate::pauseTransitionAtTime(RenderObject* renderer, const String& property, double t)
{
    if (!renderer)
        return false;

    RefPtr<CompositeAnimation> compAnim = accessCompositeAnimation(renderer);
    if (!compAnim)
        return false;

    if (compAnim->pauseTransitionAtTime(cssPropertyID(property), t)) {
        renderer->node()->setChanged(AnimationStyleChange);
        startUpdateRenderingDispatcher();
        return true;
    }

    return false;
}

double AnimationControllerPrivate::beginAnimationUpdateTime()
{
    if (m_beginAnimationUpdateTime == cBeginAnimationUpdateTimeNotSet)
        m_beginAnimationUpdateTime = currentTime();
    return m_beginAnimationUpdateTime;
}

PassRefPtr<RenderStyle> AnimationControllerPrivate::getAnimatedStyleForRenderer(RenderObject* renderer)
{
    if (!renderer)
        return 0;

    RefPtr<CompositeAnimation> rendererAnimations = m_compositeAnimations.get(renderer);
    if (!rendererAnimations)
        return renderer->style();
    
    // Make sure animationUpdateTime is updated, so that it is current even if no
    // styleChange has happened (e.g. accelerated animations).
    setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);
    RefPtr<RenderStyle> animatingStyle = rendererAnimations->getAnimatedStyle();
    if (!animatingStyle)
        animatingStyle = renderer->style();
    
    return animatingStyle.release();
}

unsigned AnimationControllerPrivate::numberOfActiveAnimations() const
{
    unsigned count = 0;
    
    RenderObjectAnimationMap::const_iterator animationsEnd = m_compositeAnimations.end();
    for (RenderObjectAnimationMap::const_iterator it = m_compositeAnimations.begin(); it != animationsEnd; ++it) {
        CompositeAnimation* compAnim = it->second.get();
        count += compAnim->numberOfActiveAnimations();
    }
    
    return count;
}

void AnimationControllerPrivate::addToStyleAvailableWaitList(AnimationBase* animation)
{
    ASSERT(!animation->next());
    
    if (m_styleAvailableWaiters)
        m_lastStyleAvailableWaiter->setNext(animation);
    else
        m_styleAvailableWaiters = animation;
        
    m_lastStyleAvailableWaiter = animation;
    animation->setNext(0);
}

void AnimationControllerPrivate::removeFromStyleAvailableWaitList(AnimationBase* animationToRemove)
{
    AnimationBase* prevAnimation = 0;
    for (AnimationBase* animation = m_styleAvailableWaiters; animation; animation = animation->next()) {
        if (animation == animationToRemove) {
            if (prevAnimation)
                prevAnimation->setNext(animation->next());
            else
                m_styleAvailableWaiters = animation->next();
            
            if (m_lastStyleAvailableWaiter == animation)
                m_lastStyleAvailableWaiter = prevAnimation;
                
            animationToRemove->setNext(0);
        }
    }
}

void AnimationControllerPrivate::styleAvailable()
{
    // Go through list of waiters and send them on their way
    for (AnimationBase* animation = m_styleAvailableWaiters; animation; ) {
        AnimationBase* nextAnimation = animation->next();
        animation->setNext(0);
        animation->styleAvailable();
        animation = nextAnimation;
    }
    
    m_styleAvailableWaiters = 0;
    m_lastStyleAvailableWaiter = 0;
}

void AnimationControllerPrivate::addToStartTimeResponseWaitList(AnimationBase* animation, bool willGetResponse)
{
    // If willGetResponse is true, it means this animation is actually waiting for a response
    // (which will come in as a call to notifyAnimationStarted()).
    // In that case we don't need to add it to this list. We just set a waitingForAResponse flag 
    // which says we are waiting for the response. If willGetResponse is false, this animation 
    // is not waiting for a response for itself, but rather for a notifyXXXStarted() call for 
    // another animation to which it will sync.
    //
    // When endAnimationUpdate() is called we check to see if the waitingForAResponse flag is
    // true. If so, we just return and will do our work when the first notifyXXXStarted() call
    // comes in. If it is false, we will not be getting a notifyXXXStarted() call, so we will
    // do our work right away. In both cases we call the onAnimationStartResponse() method
    // on each animation. In the first case we send in the time we got from notifyXXXStarted().
    // In the second case, we just pass in the beginAnimationUpdateTime().
    //
    // This will synchronize all software and accelerated animations started in the same 
    // updateRendering cycle.
    //
    ASSERT(!animation->next());
    
    if (willGetResponse)
        m_waitingForAResponse = true;
    
    if (m_responseWaiters)
        m_lastResponseWaiter->setNext(animation);
    else
        m_responseWaiters = animation;
        
    m_lastResponseWaiter = animation;
    animation->setNext(0);
}

void AnimationControllerPrivate::removeFromStartTimeResponseWaitList(AnimationBase* animationToRemove)
{
    AnimationBase* prevAnimation = 0;
    for (AnimationBase* animation = m_responseWaiters; animation; animation = animation->next()) {
        if (animation == animationToRemove) {
            if (prevAnimation)
                prevAnimation->setNext(animation->next());
            else
                m_responseWaiters = animation->next();
            
            if (m_lastResponseWaiter == animation)
                m_lastResponseWaiter = prevAnimation;
                
            animationToRemove->setNext(0);
        }
        prevAnimation = animation;
    }
}

void AnimationControllerPrivate::startTimeResponse(double t)
{
    // Go through list of waiters and send them on their way
    for (AnimationBase* animation = m_responseWaiters; animation; ) {
        AnimationBase* nextAnimation = animation->next();
        animation->setNext(0);
        animation->onAnimationStartResponse(t);
        animation = nextAnimation;
    }
    
    m_responseWaiters = 0;
    m_lastResponseWaiter = 0;
}

AnimationController::AnimationController(Frame* frame)
    : m_data(new AnimationControllerPrivate(frame))
{
}

AnimationController::~AnimationController()
{
    delete m_data;
}

void AnimationController::cancelAnimations(RenderObject* renderer)
{
    if (!m_data->hasAnimations())
        return;

    if (m_data->clear(renderer)) {
        Node* node = renderer->element();
        ASSERT(!node || (node->document() && !node->document()->inPageCache()));
        node->setChanged(AnimationStyleChange);
    }
}

PassRefPtr<RenderStyle> AnimationController::updateAnimations(RenderObject* renderer, RenderStyle* newStyle)
{
    // Don't do anything if we're in the cache
    if (!renderer->document() || renderer->document()->inPageCache())
        return newStyle;

    RenderStyle* oldStyle = renderer->style();

    if ((!oldStyle || (!oldStyle->animations() && !oldStyle->transitions())) && (!newStyle->animations() && !newStyle->transitions()))
        return newStyle;

    // Fetch our current set of implicit animations from a hashtable.  We then compare them
    // against the animations in the style and make sure we're in sync.  If destination values
    // have changed, we reset the animation.  We then do a blend to get new values and we return
    // a new style.
    ASSERT(renderer->element()); // FIXME: We do not animate generated content yet.

    RefPtr<CompositeAnimation> rendererAnimations = m_data->accessCompositeAnimation(renderer);
    RefPtr<RenderStyle> blendedStyle = rendererAnimations->animate(renderer, oldStyle, newStyle);

    m_data->updateAnimationTimer();

    if (blendedStyle != newStyle) {
        // If the animations/transitions change opacity or transform, we neeed to update
        // the style to impose the stacking rules. Note that this is also
        // done in CSSStyleSelector::adjustRenderStyle().
        if (blendedStyle->hasAutoZIndex() && (blendedStyle->opacity() < 1.0f || blendedStyle->hasTransform()))
            blendedStyle->setZIndex(0);
    }
    return blendedStyle.release();
}

PassRefPtr<RenderStyle> AnimationController::getAnimatedStyleForRenderer(RenderObject* renderer)
{
    return m_data->getAnimatedStyleForRenderer(renderer);
}

void AnimationController::notifyAnimationStarted(RenderObject*, double startTime)
{
    m_data->receivedStartTimeResponse(startTime);
}

bool AnimationController::pauseAnimationAtTime(RenderObject* renderer, const String& name, double t)
{
    return m_data->pauseAnimationAtTime(renderer, name, t);
}

unsigned AnimationController::numberOfActiveAnimations() const
{
    return m_data->numberOfActiveAnimations();
}

bool AnimationController::pauseTransitionAtTime(RenderObject* renderer, const String& property, double t)
{
    return m_data->pauseTransitionAtTime(renderer, property, t);
}

bool AnimationController::isAnimatingPropertyOnRenderer(RenderObject* renderer, CSSPropertyID property, bool isRunningNow) const
{
    return m_data->isAnimatingPropertyOnRenderer(renderer, property, isRunningNow);
}

void AnimationController::suspendAnimations(Document* document)
{
    m_data->suspendAnimations(document);
}

void AnimationController::resumeAnimations(Document* document)
{
    m_data->resumeAnimations(document);
}

void AnimationController::beginAnimationUpdate()
{
    m_data->setBeginAnimationUpdateTime(cBeginAnimationUpdateTimeNotSet);
}

void AnimationController::endAnimationUpdate()
{
    m_data->endAnimationUpdate();
}

bool AnimationController::supportsAcceleratedAnimationOfProperty(CSSPropertyID property)
{
#if USE(ACCELERATED_COMPOSITING)
    return AnimationBase::animationOfPropertyIsAccelerated(property);
#else
    UNUSED_PARAM(property);
    return false;
#endif
}

} // namespace WebCore
