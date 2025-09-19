/////////////////////////////////////////////////////////////////////////////
// Name:        Otherstreams.h
// Purpose:     xz and lmza1 archive streams
// Part of:     4Pane
// Author:      David Hart
// Copyright:   (c) 2016 David Hart
// Licence:     GPL v3
/////////////////////////////////////////////////////////////////////////////

#ifndef _OTHERSTREAMS_H__
#define _OTHERSTREAMS_H__

#ifndef NO_LZMA_ARCHIVE_STREAMS // Allows compilation where liblzma-dev/xz-libs xz-devel is unavailable

#include "wx/defs.h"

#if wxUSE_STREAMS

#include "wx/stream.h"
#include <lzma.h>

#ifndef IN_BUF_MAX
  #define IN_BUF_MAX	4096
  #define OUT_BUF_MAX	4096
#endif

typedef lzma_ret  (* dl_lzma_stream_decoder) (lzma_stream*, uint64_t, uint32_t);
typedef lzma_ret  (* dl_lzma_alone_decoder)  (lzma_stream*, uint64_t);
typedef lzma_ret  (* dl_lzma_code)           (lzma_stream*, lzma_action);
typedef void      (* dl_lzma_end)            (lzma_stream*);
typedef lzma_ret  (* dl_lzma_easy_encoder)   (lzma_stream*, uint32_t, lzma_check);
typedef lzma_bool (* dl_lzma_lzma_preset)    (lzma_options_lzma*, uint32_t);
typedef lzma_ret  (* dl_lzma_alone_encoder)  (lzma_stream*, const lzma_options_lzma*);

class XzInputStream : public wxFilterInputStream
{
public:

  XzInputStream(wxInputStream& stream, bool use_lzma1 = false); 
  virtual ~XzInputStream();

  wxInputStream& ReadRaw(void* pBuffer, size_t size);
  off_t TellRawI();
  off_t SeekRawI(off_t pos, wxSeekMode sm = wxFromStart);

  void* GetHandleI() {return m_lzStream;}
protected:
  virtual size_t OnSysRead(void *buffer, size_t bufsize);

  // dlopen()ed symbols
  dl_lzma_stream_decoder lzma_stream_decoder;
  dl_lzma_alone_decoder  lzma_alone_decoder;
  dl_lzma_code           lzma_code;
  dl_lzma_end            lzma_end;

  void*   m_lzStream;
  unsigned char m_pBuffer[IN_BUF_MAX];
  int     m_nBufferPos;
};

class XzOutputStream : public wxFilterOutputStream
{
public:
  XzOutputStream(wxOutputStream& stream, bool use_lzma1 = false);
  virtual ~XzOutputStream();

  wxOutputStream& WriteRaw(void* pBuffer, size_t size);
  off_t TellRawO();
  off_t SeekRawO(off_t pos, wxSeekMode sm = wxFromStart);
     
  void* GetHandleO() {return m_lzStream;}

  virtual bool  Close();

protected:
  virtual size_t OnSysWrite(const void *buffer, size_t bufsize);

  // dlopen()ed symbols  
  dl_lzma_easy_encoder  lzma_easy_encoder;
  dl_lzma_lzma_preset   lzma_lzma_preset;
  dl_lzma_alone_encoder lzma_alone_encoder;
  dl_lzma_code          lzma_code;
  dl_lzma_end           lzma_end;
  
  void*   m_lzStream;
  char m_pBuffer[OUT_BUF_MAX];
  int     m_nBufferPos;
};

class XzStream : public XzInputStream, XzOutputStream
{
public:
  XzStream(wxInputStream& istream, wxOutputStream& ostream) :  XzInputStream(istream), XzOutputStream(ostream) {}
  virtual ~XzStream(){}
};


class WXEXPORT XzClassFactory: public wxFilterClassFactory
{
public:
    XzClassFactory();

    wxFilterInputStream *NewStream(wxInputStream& stream) const
        { return new XzInputStream(stream); }
    wxFilterOutputStream *NewStream(wxOutputStream& stream) const
        { return new XzOutputStream(stream); }
    wxFilterInputStream *NewStream(wxInputStream *stream) const
        { return new XzInputStream(*stream); }
    wxFilterOutputStream *NewStream(wxOutputStream *stream) const
        { return new XzOutputStream(*stream); }

    const wxChar * const *GetProtocols(wxStreamProtocolType type
                                       = wxSTREAM_PROTOCOL) const;

private:
    DECLARE_DYNAMIC_CLASS(XzClassFactory)
};

// Lzma streams. These use liblzma too, just like xz
// The only difference is in the en/decoder initialisation, so we only need to pass a bool to the xzstream ctors
class LzmaInputStream : public XzInputStream
{
public:
  LzmaInputStream(wxInputStream& stream) : XzInputStream(stream, true) {}
  virtual ~LzmaInputStream() {}
};

class LzmaOutputStream : public XzOutputStream
{
public:
  LzmaOutputStream(wxOutputStream& stream) : XzOutputStream(stream, true) {}
  virtual ~LzmaOutputStream() {}
};


#endif // wxUSE_STREAMS
#endif // ndef NO_LZMA_ARCHIVE_STREAMS
#endif // _OTHERSTREAMS_H__
