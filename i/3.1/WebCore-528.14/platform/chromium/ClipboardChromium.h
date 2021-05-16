// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef ClipboardChromium_h
#define ClipboardChromium_h

#include "Clipboard.h"

#include "CachedResourceClient.h"

namespace WebCore {

    class CachedImage;
    class ChromiumDataObject;
    class IntPoint;

    class ClipboardChromium : public Clipboard, public CachedResourceClient {
    public:
        ~ClipboardChromium() {}

        static PassRefPtr<ClipboardChromium> create(
            bool isForDragging, PassRefPtr<ChromiumDataObject>, ClipboardAccessPolicy);

        virtual void clearData(const String& type);
        void clearAllData();
        String getData(const String& type, bool& success) const;
        bool setData(const String& type, const String& data);

        // extensions beyond IE's API
        HashSet<String> types() const;

        void setDragImage(CachedImage*, const IntPoint&);
        void setDragImageElement(Node*, const IntPoint&);

        PassRefPtr<ChromiumDataObject> dataObject()
        {
            return m_dataObject;
        }

        virtual DragImageRef createDragImage(IntPoint& dragLoc) const;
        virtual void declareAndWriteDragImage(Element*, const KURL&, const String& title, Frame*);
        virtual void writeURL(const KURL&, const String&, Frame*);
        virtual void writeRange(Range*, Frame*);

        virtual bool hasData();

    private:
        ClipboardChromium(bool, PassRefPtr<ChromiumDataObject>, ClipboardAccessPolicy);

        void resetFromClipboard();
        void setDragImage(CachedImage*, Node*, const IntPoint&);
        RefPtr<ChromiumDataObject> m_dataObject;
        Frame* m_frame;
    };

} // namespace WebCore

#endif // ClipboardChromium_h
