/*
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSLazyEventListener.h"

#include "Frame.h"
#include "JSEventTargetNode.h"
#include <runtime/FunctionConstructor.h>

using namespace JSC;

namespace WebCore {

JSLazyEventListener::JSLazyEventListener(LazyEventListenerType type, const String& functionName, const String& code, JSDOMGlobalObject* globalObject, Node* node, int lineNumber)
    : JSProtectedEventListener(0, globalObject, true)
    , m_functionName(functionName)
    , m_code(code)
    , m_parsed(false)
    , m_lineNumber(lineNumber)
    , m_originalNode(node)
    , m_type(type)
{
    // We don't retain the original node because we assume it
    // will stay alive as long as this handler object is around
    // and we need to avoid a reference cycle. If JS transfers
    // this handler to another node, parseCode will be called and
    // then originalNode is no longer needed.

    // A JSLazyEventListener can be created with a line number of zero when it is created with
    // a setAttribute call from JavaScript, so make the line number 1 in that case.
    if (m_lineNumber == 0)
        m_lineNumber = 1;
}

JSObject* JSLazyEventListener::function() const
{
    parseCode();
    return m_listener;
}

static inline JSValuePtr eventParameterName(JSLazyEventListener::LazyEventListenerType type, ExecState* exec)
{
    switch (type) {
        case JSLazyEventListener::HTMLLazyEventListener:
            return jsNontrivialString(exec, "event");
#if ENABLE(SVG)
        case JSLazyEventListener::SVGLazyEventListener:
            return jsNontrivialString(exec, "evt");
#endif
    }
    ASSERT_NOT_REACHED();
    return jsUndefined();
}

void JSLazyEventListener::parseCode() const
{
    if (m_parsed)
        return;

    if (m_globalObject->scriptExecutionContext()->isDocument()) {
        JSDOMWindow* window = static_cast<JSDOMWindow*>(m_globalObject.get());
        Frame* frame = window->impl()->frame();
        if (!frame)
            return;
        // FIXME: Is this check needed for non-Document contexts?
        ScriptController* script = frame->script();
        if (!script->isEnabled() || script->isPaused())
            return;
    }

    m_parsed = true;

    ExecState* exec = m_globalObject->globalExec();

    ArgList args;
    UString sourceURL(m_globalObject->scriptExecutionContext()->url().string());
    args.append(eventParameterName(m_type, exec));
    args.append(jsString(exec, m_code));

    // FIXME: Passing the document's URL to construct is not always correct, since this event listener might
    // have been added with setAttribute from a script, and we should pass String() in that case.
    m_listener = constructFunction(exec, args, Identifier(exec, m_functionName), sourceURL, m_lineNumber); // FIXME: is globalExec ok?

    JSFunction* listenerAsFunction = static_cast<JSFunction*>(m_listener.get());

    if (exec->hadException()) {
        exec->clearException();

        // failed to parse, so let's just make this listener a no-op
        m_listener = 0;
    } else if (m_originalNode) {
        // Add the event's home element to the scope
        // (and the document, and the form - see JSHTMLElement::eventHandlerScope)
        ScopeChain scope = listenerAsFunction->scope();

        JSValuePtr thisObj = toJS(exec, m_originalNode);
        if (thisObj.isObject()) {
            static_cast<JSEventTargetNode*>(asObject(thisObj))->pushEventHandlerScope(exec, scope);
            listenerAsFunction->setScope(scope);
        }
    }

    // no more need to keep the unparsed code around
    m_functionName = String();
    m_code = String();

    if (m_listener) {
        ASSERT(isInline());
        JSDOMWindow::ProtectedListenersMap& listeners = m_globalObject->jsProtectedInlineEventListeners();
        listeners.set(m_listener, const_cast<JSLazyEventListener*>(this));
    }
}

} // namespace WebCore
