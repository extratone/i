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
#include "CompositeAnimation.h"

#include "AnimationControllerPrivate.h"
#include "CSSPropertyNames.h"
#include "ImplicitAnimation.h"
#include "KeyframeAnimation.h"
#include "RenderObject.h"
#include "RenderStyle.h"

namespace WebCore {

CompositeAnimation::~CompositeAnimation()
{
    // Toss the refs to all animations
    m_transitions.clear();
    m_keyframeAnimations.clear();
}

void CompositeAnimation::clearRenderer()
{
    if (!m_transitions.isEmpty()) {
        // Clear the renderers from all running animations, in case we are in the middle of
        // an animation callback (see https://bugs.webkit.org/show_bug.cgi?id=22052)
        CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
            ImplicitAnimation* transition = it->second.get();
            transition->clearRenderer();
        }
    }
    if (!m_keyframeAnimations.isEmpty()) {
        AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
        for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
            KeyframeAnimation* anim = it->second.get();
            anim->clearRenderer();
        }
    }
}

void CompositeAnimation::updateTransitions(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    RefPtr<RenderStyle> modifiedCurrentStyle;
    
    // If currentStyle is null, we don't do transitions
    if (!currentStyle || !targetStyle->transitions())
        return;

    // Check to see if we need to update the active transitions
    for (size_t i = 0; i < targetStyle->transitions()->size(); ++i) {
        const Animation* anim = targetStyle->transitions()->animation(i);
        bool isActiveTransition = anim->duration() || anim->delay() > 0;

        int prop = anim->property();

        if (prop == cAnimateNone)
            continue;

        bool all = prop == cAnimateAll;

        // Handle both the 'all' and single property cases. For the single prop case, we make only one pass
        // through the loop.
        for (int propertyIndex = 0; propertyIndex < AnimationBase::getNumProperties(); ++propertyIndex) {
            if (all) {
                // Get the next property which is not a shorthand.
                bool isShorthand;
                prop = AnimationBase::getPropertyAtIndex(propertyIndex, isShorthand);
                if (isShorthand)
                    continue;
            }

            // ImplicitAnimations are always hashed by actual properties, never cAnimateAll
            ASSERT(prop >= firstCSSProperty && prop < (firstCSSProperty + numCSSProperties));

            // If there is a running animation for this property, the transition is overridden
            // and we have to use the unanimatedStyle from the animation. We do the test
            // against the unanimated style here, but we "override" the transition later.
            RefPtr<KeyframeAnimation> keyframeAnim = getAnimationForProperty(prop);
            RenderStyle* fromStyle = keyframeAnim ? keyframeAnim->unanimatedStyle() : currentStyle;

            // See if there is a current transition for this prop
            ImplicitAnimation* implAnim = m_transitions.get(prop).get();
            bool equal = true;

            if (implAnim) {
                // This might be a transition that is just finishing. That would be the case
                // if it were postActive. But we still need to check for equality because
                // it could be just finishing AND changing to a new goal state.
                //
                // This implAnim might also not be an already running transition. It might be
                // newly added to the list in a previous iteration. This would happen if
                // you have both an explicit transition-property and 'all' in the same
                // list. In this case, the latter one overrides the earlier one, so we
                // behave as though this is a running animation being replaced.
                if (!implAnim->isTargetPropertyEqual(prop, targetStyle)) {
#if USE(ACCELERATED_COMPOSITING)
                    // For accelerated animations we need to return a new RenderStyle with the _current_ value
                    // of the property, so that restarted transitions use the correct starting point.
                    if (AnimationBase::animationOfPropertyIsAccelerated(prop) && !implAnim->isFallbackAnimating()) {
                        if (!modifiedCurrentStyle)
                            modifiedCurrentStyle = RenderStyle::clone(currentStyle);

                        implAnim->blendPropertyValueInStyle(prop, modifiedCurrentStyle.get());
                    }
#endif
                    m_transitions.remove(prop);
                    equal = false;
                }
            } else {
                // We need to start a transition if it is active and the properties don't match
                equal = !isActiveTransition || AnimationBase::propertiesEqual(prop, fromStyle, targetStyle);
            }

            // We can be in this loop with an inactive transition (!isActiveTransition). We need
            // to do that to check to see if we are canceling a transition. But we don't want to
            // start one of the inactive transitions. So short circuit that here. (See
            // <https://bugs.webkit.org/show_bug.cgi?id=24787>
            if (!equal && isActiveTransition) {
                // Add the new transition
                m_transitions.set(prop, ImplicitAnimation::create(const_cast<Animation*>(anim), prop, renderer, this, modifiedCurrentStyle ? modifiedCurrentStyle.get() : fromStyle));
            }
            
            // We only need one pass for the single prop case
            if (!all)
                break;
        }
    }
}

void CompositeAnimation::updateKeyframeAnimations(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    // Nothing to do if we don't have any animations, and didn't have any before
    if (m_keyframeAnimations.isEmpty() && !targetStyle->hasAnimations())
        return;

    // Nothing to do if the current and target animations are the same
    if (currentStyle && currentStyle->hasAnimations() && targetStyle->hasAnimations() && *(currentStyle->animations()) == *(targetStyle->animations()))
        return;
        
    // Mark all existing animations as no longer active
    AnimationNameMap::const_iterator kfend = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it)
        it->second->setIndex(-1);
        
    // Toss the animation order map
    m_keyframeAnimationOrderMap.clear();
    
    // Now mark any still active animations as active and add any new animations
    if (targetStyle->animations()) {
        int numAnims = targetStyle->animations()->size();
        for (int i = 0; i < numAnims; ++i) {
            const Animation* anim = targetStyle->animations()->animation(i);
            AtomicString animationName(anim->name());

            if (!anim->isValidAnimation())
                continue;
            
            // See if there is a current animation for this name
            RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(animationName.impl());
                
            if (keyframeAnim) {
                // There is one so it is still active

                // Animations match, but play states may differ. update if needed
                keyframeAnim->updatePlayState(anim->playState() == AnimPlayStatePlaying);
                            
                // Set the saved animation to this new one, just in case the play state has changed
                keyframeAnim->setAnimation(anim);
                keyframeAnim->setIndex(i);
            } else if ((anim->duration() || anim->delay()) && anim->iterationCount()) {
                keyframeAnim = KeyframeAnimation::create(const_cast<Animation*>(anim), renderer, i, this, currentStyle ? currentStyle : targetStyle);
                m_keyframeAnimations.set(keyframeAnim->name().impl(), keyframeAnim);
            }
            
            // Add this to the animation order map
            if (keyframeAnim)
                m_keyframeAnimationOrderMap.append(keyframeAnim->name().impl());
        }
    }

    // Make a list of animations to be removed
    Vector<AtomicStringImpl*> animsToBeRemoved;
    kfend = m_keyframeAnimations.end();
    for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != kfend; ++it) {
        KeyframeAnimation* keyframeAnim = it->second.get();
        if (keyframeAnim->index() < 0)
            animsToBeRemoved.append(keyframeAnim->name().impl());
    }
    
    // Now remove the animations from the list
    for (size_t j = 0; j < animsToBeRemoved.size(); ++j)
        m_keyframeAnimations.remove(animsToBeRemoved[j]);
}

PassRefPtr<RenderStyle> CompositeAnimation::animate(RenderObject* renderer, RenderStyle* currentStyle, RenderStyle* targetStyle)
{
    RefPtr<RenderStyle> resultStyle;

    // Update animations first so we can see if any transitions are overridden
    updateKeyframeAnimations(renderer, currentStyle, targetStyle);

    // We don't do any transitions if we don't have a currentStyle (on startup)
    updateTransitions(renderer, currentStyle, targetStyle);

    if (currentStyle) {
        // Now that we have transition objects ready, let them know about the new goal state.  We want them
        // to fill in a RenderStyle*& only if needed.
        if (!m_transitions.isEmpty()) {
            CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
            for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
                if (ImplicitAnimation* anim = it->second.get())
                    anim->animate(this, renderer, currentStyle, targetStyle, resultStyle);
            }
        }
    }

    // Now that we have animation objects ready, let them know about the new goal state.  We want them
    // to fill in a RenderStyle*& only if needed.
    for (Vector<AtomicStringImpl*>::const_iterator it = m_keyframeAnimationOrderMap.begin(); it != m_keyframeAnimationOrderMap.end(); ++it) {
        RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(*it);
        if (keyframeAnim)
            keyframeAnim->animate(this, renderer, currentStyle, targetStyle, resultStyle);
    }

    cleanupFinishedAnimations();

    return resultStyle ? resultStyle.release() : targetStyle;
}

PassRefPtr<RenderStyle> CompositeAnimation::getAnimatedStyle() const
{
    RefPtr<RenderStyle> resultStyle;
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
        if (ImplicitAnimation* implicitAnimation = it->second.get())
            implicitAnimation->getAnimatedStyle(resultStyle);
    }

    for (Vector<AtomicStringImpl*>::const_iterator it = m_keyframeAnimationOrderMap.begin(); it != m_keyframeAnimationOrderMap.end(); ++it) {
        RefPtr<KeyframeAnimation> keyframeAnimation = m_keyframeAnimations.get(*it);
        if (keyframeAnimation)
            keyframeAnimation->getAnimatedStyle(resultStyle);
    }
    
    return resultStyle;
}

// "animating" means that something is running that requires the timer to keep firing
void CompositeAnimation::setAnimating(bool animating)
{
    if (!m_transitions.isEmpty()) {
        CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
            ImplicitAnimation* transition = it->second.get();
            transition->setAnimating(animating);
        }
    }
    if (!m_keyframeAnimations.isEmpty()) {
        AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
        for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
            KeyframeAnimation* anim = it->second.get();
            anim->setAnimating(animating);
        }
    }
}

double CompositeAnimation::timeToNextService() const
{
    // Returns the time at which next service is required. -1 means no service is required. 0 means 
    // service is required now, and > 0 means service is required that many seconds in the future.
    double minT = -1;
    
    if (!m_transitions.isEmpty()) {
        CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
            ImplicitAnimation* transition = it->second.get();
            double t = transition ? transition->timeToNextService() : -1;
            if (t < minT || minT == -1)
                minT = t;
            if (minT == 0)
                return 0;
        }
    }
    if (!m_keyframeAnimations.isEmpty()) {
        AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
        for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
            KeyframeAnimation* animation = it->second.get();
            double t = animation ? animation->timeToNextService() : -1;
            if (t < minT || minT == -1)
                minT = t;
            if (minT == 0)
                return 0;
        }
    }

    return minT;
}

PassRefPtr<KeyframeAnimation> CompositeAnimation::getAnimationForProperty(int property) const
{
    RefPtr<KeyframeAnimation> retval;
    
    // We want to send back the last animation with the property if there are multiples.
    // So we need to iterate through all animations
    if (!m_keyframeAnimations.isEmpty()) {
        AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
        for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
            RefPtr<KeyframeAnimation> anim = it->second;
            if (anim->hasAnimationForProperty(property))
                retval = anim;
        }
    }
    
    return retval;
}

void CompositeAnimation::cleanupFinishedAnimations()
{
    if (isSuspended())
        return;

    // Make a list of transitions to be deleted
    Vector<int> finishedTransitions;
    if (!m_transitions.isEmpty()) {
        CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();

        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
            ImplicitAnimation* anim = it->second.get();
            if (!anim)
                continue;
            if (anim->postActive())
                finishedTransitions.append(anim->animatingProperty());
        }
        
        // Delete them
        size_t finishedTransitionCount = finishedTransitions.size();
        for (size_t i = 0; i < finishedTransitionCount; ++i)
            m_transitions.remove(finishedTransitions[i]);
    }

    // Make a list of animations to be deleted
    Vector<AtomicStringImpl*> finishedAnimations;
    if (!m_keyframeAnimations.isEmpty()) {
        AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();

        for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
            KeyframeAnimation* anim = it->second.get();
            if (!anim)
                continue;
            if (anim->postActive())
                finishedAnimations.append(anim->name().impl());
        }

        // Delete them
        size_t finishedAnimationCount = finishedAnimations.size();
        for (size_t i = 0; i < finishedAnimationCount; ++i)
            m_keyframeAnimations.remove(finishedAnimations[i]);
    }
}

void CompositeAnimation::suspendAnimations()
{
    if (m_isSuspended)
        return;

    m_isSuspended = true;

    if (!m_keyframeAnimations.isEmpty()) {
        AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
        for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
            if (KeyframeAnimation* anim = it->second.get())
                anim->updatePlayState(false);
        }
    }
    if (!m_transitions.isEmpty()) {
        CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
            ImplicitAnimation* anim = it->second.get();
            if (anim && anim->hasStyle())
                anim->updatePlayState(false);
        }
    }
}

void CompositeAnimation::resumeAnimations()
{
    if (!m_isSuspended)
        return;

    m_isSuspended = false;

    if (!m_keyframeAnimations.isEmpty()) {
        AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
        for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
            KeyframeAnimation* anim = it->second.get();
            if (anim && anim->playStatePlaying())
                anim->updatePlayState(true);
        }
    }

    if (!m_transitions.isEmpty()) {
        CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
            ImplicitAnimation* anim = it->second.get();
            if (anim && anim->hasStyle())
                anim->updatePlayState(true);
        }
    }
}

void CompositeAnimation::overrideImplicitAnimations(int property)
{
    CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
    if (!m_transitions.isEmpty()) {
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
            ImplicitAnimation* anim = it->second.get();
            if (anim && anim->animatingProperty() == property)
                anim->setOverridden(true);
        }
    }
}

void CompositeAnimation::resumeOverriddenImplicitAnimations(int property)
{
    if (!m_transitions.isEmpty()) {
        CSSPropertyTransitionsMap::const_iterator end = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != end; ++it) {
            ImplicitAnimation* anim = it->second.get();
            if (anim && anim->animatingProperty() == property)
                anim->setOverridden(false);
        }
    }
}

bool CompositeAnimation::isAnimatingProperty(int property, bool isRunningNow) const
{
    if (!m_keyframeAnimations.isEmpty()) {
        AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
        for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
            KeyframeAnimation* anim = it->second.get();
            if (anim && anim->isAnimatingProperty(property, isRunningNow))
                return true;
        }
    }

    if (!m_transitions.isEmpty()) {
        CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
            ImplicitAnimation* anim = it->second.get();
            if (anim && anim->isAnimatingProperty(property, isRunningNow))
                return true;
        }
    }
    return false;
}

bool CompositeAnimation::pauseAnimationAtTime(const AtomicString& name, double t)
{
    if (!name)
        return false;

    RefPtr<KeyframeAnimation> keyframeAnim = m_keyframeAnimations.get(name.impl());
    if (!keyframeAnim || !keyframeAnim->running())
        return false;

    int count = keyframeAnim->m_animation->iterationCount();
    if ((t >= 0.0) && (!count || (t <= count * keyframeAnim->duration()))) {
        keyframeAnim->pauseAtTime(t);
        return true;
    }

    return false;
}

bool CompositeAnimation::pauseTransitionAtTime(int property, double t)
{
    if ((property < firstCSSProperty) || (property >= firstCSSProperty + numCSSProperties))
        return false;

    ImplicitAnimation* implAnim = m_transitions.get(property).get();
    if (!implAnim || !implAnim->running())
        return false;

    if ((t >= 0.0) && (t <= implAnim->duration())) {
        implAnim->pauseAtTime(t);
        return true;
    }

    return false;
}

unsigned CompositeAnimation::numberOfActiveAnimations() const
{
    unsigned count = 0;
    
    if (!m_keyframeAnimations.isEmpty()) {
        AnimationNameMap::const_iterator animationsEnd = m_keyframeAnimations.end();
        for (AnimationNameMap::const_iterator it = m_keyframeAnimations.begin(); it != animationsEnd; ++it) {
            KeyframeAnimation* anim = it->second.get();
            if (anim->running())
                ++count;
        }
    }

    if (!m_transitions.isEmpty()) {
        CSSPropertyTransitionsMap::const_iterator transitionsEnd = m_transitions.end();
        for (CSSPropertyTransitionsMap::const_iterator it = m_transitions.begin(); it != transitionsEnd; ++it) {
            ImplicitAnimation* anim = it->second.get();
            if (anim->running())
                ++count;
        }
    }
    
    return count;
}

} // namespace WebCore
